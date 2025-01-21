#define GLFW_INCLUDE_NONE

#include "GBCore.h"
#include "gbSystem.h"
#include "appConfig.h"
#include "keyBindManager.h"
#include "debugUI.h"
#include "resources.h"
#include "Utils/Shader.h"
#include "Utils/fileUtils.h"
#include "Utils/glFunctions.h"
#include "Utils/memstream.h"

#include <iostream>
#include <filesystem>
#include <span>

#include <ImGUI/imgui.h>
#include <ImGUI/imgui_internal.h>
#include <ImGUI/imgui_impl_glfw.h>
#include <ImGUI/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <miniz/miniz.h>

#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <emscripten_browser_file/emscripten_browser_file.h>

/* Currently not using Emscripten GLFW contrib port due to issues with imgui hidpi font scaling there, which resulted in text appearing blurry.
   Instead, the official GLFW port is used for now. However, it has a lot of features (cursors, clipboard, etc.) unimplemented,
   so I implement them myself in this app only when EMSCRIPTEN_DEFAULT_GLFW is defined, in case I switch to the contrib port in the future. */

#ifndef EMSCRIPTEN_USE_PORT_CONTRIB_GLFW3
#define EMSCRIPTEN_DEFAULT_GLFW
#include <emscripten_browser_clipboard/emscripten_browser_clipboard.h>
#endif

#else
#include <glad/glad.h>
#include <nfd.hpp>
#include <thread>
#endif

static_assert(std::endian::native == std::endian::little, "This program requires a little-endian architecture.");
static_assert(CHAR_BIT == 8, "This program requires 'char' to be 8 bits in size.");

constexpr const char* APP_NAME = "MegaBoy";

GBCore gb;
GLFWwindow* window{};

int menuBarHeight{};
int window_width{}, window_height{};
int viewport_width{}, viewport_height{};
int viewport_xOffset{}, viewport_yOffset{};
float scaleFactor{};

int currentIntegerScale{};
int newIntegerScale { -1 };

Shader regularShader{};
Shader scalingShader{};
Shader lcdShader{};

Shader* currentShader{ };
std::array<uint32_t, 2> gbFramebufferTextures{};

bool glScreenshotRequested { false };
bool fileDialogOpen { false };

#ifndef EMSCRIPTEN
constexpr nfdnfilteritem_t openFilterItem[] { { N_STR("Game ROM/Save"), N_STR("gb,gbc,zip,sav,mbs,bin") } };
constexpr nfdnfilteritem_t saveStateFilterItem[] { { N_STR("Save State"), N_STR("mbs") } };
constexpr nfdnfilteritem_t batterySaveFilterItem[] { { N_STR("Battery Save"), N_STR("sav") } };
constexpr nfdnfilteritem_t audioSaveFilterItem[] { { N_STR("WAV File"), N_STR("wav") } };
#else
constexpr const char* openFilterItem { ".gb,.gbc,.zip,.sav,.mbs,.bin" };

double devicePixelRatio{};
bool emscripten_saves_syncing { false };
#endif

const char* popupTitle { "" };
bool showInfoPopUp { false };

std::string FPS_text{ "FPS: 00.00 - 0.00 ms" };

constexpr float FADE_DURATION { 0.9f };
bool fadeEffectActive { false };
float fadeTime { 0.0f };

bool fastForwarding { false };
constexpr int FAST_FORWARD_SPEED = 5;

bool lockVSyncSetting { false };

int awaitingKeyBind { -1 };

constexpr int NUM_SAVE_STATES { 10 };
std::array<bool, NUM_SAVE_STATES> modifiedSaveStates{};
bool showSaveStatePopUp{ false };

inline bool emulationRunning()
{
    const bool isRunning = !gb.emulationPaused && gb.cartridge.ROMLoaded() && !gb.breakpointHit;

#ifdef EMSCRIPTEN
    return isRunning && !emscripten_saves_syncing;
#else
    return isRunning;
#endif
}

inline void updateWindowTitle()
{
    std::string title = (gb.gameTitle.empty() ? APP_NAME : "MegaBoy - " + gb.gameTitle);
#ifndef EMSCRIPTEN
    if (emulationRunning())
        title += " (" + FPS_text + ")";
#endif
    glfwSetWindowTitle(window, title.c_str());
}

inline void setEmulationPaused(bool val)
{
    gb.emulationPaused = val;
    updateWindowTitle();
}
inline void resetRom(bool fullReset)
{
    gb.resetRom(fullReset);
    debugUI::signalROMreset();
}

#ifdef EMSCRIPTEN
extern "C" EMSCRIPTEN_KEEPALIVE void emscripten_on_save_finish_sync()
{
    gb.loadCurrentBatterySave();
    emscripten_saves_syncing = false;
    std::ranges::fill(modifiedSaveStates, true);
}
#endif

inline void activateInfoPopUp(const char* title)
{
    popupTitle = title;
	showInfoPopUp = true;
    showSaveStatePopUp = false;
}

void handleBootROMLoad(GBSystem sys, std::istream& st, const std::filesystem::path& filePath)
{
    if (GBCore::isBootROMValid(st, filePath))
    {
        auto& destPath = sys == GBSystem::DMG ? appConfig::dmgBootRomPath : appConfig::cgbBootRomPath;
#ifdef EMSCRIPTEN
        std::ofstream destFile { destPath, std::ios::binary | std::ios::out };
        destFile << st.rdbuf();
#else
        destPath = filePath;
        appConfig::updateConfigFile();
#endif
        activateInfoPopUp("Successfully Loaded Boot ROM!");
    }
    else
        activateInfoPopUp("Invalid Boot ROM!");
}

bool loadFile(std::istream& st, const std::filesystem::path& filePath)
{
    static const std::map<std::filesystem::path, GBSystem> bootRomMap = 
    {
        { GBCore::DMG_BOOTROM_NAME, GBSystem::DMG },
        { GBCore::CGB_BOOTROM_NAME, GBSystem::GBC },
    };

    if (auto it = bootRomMap.find(filePath.filename()); it != bootRomMap.end()) 
    {
        handleBootROMLoad(it->second, st, filePath);
        return true;
    }

    const auto handleFileError = [](const char* errorMessage) -> bool
    {
        activateInfoPopUp(errorMessage);

        if (!gb.cartridge.ROMLoaded())
        {
            appConfig::romPath.clear();
            appConfig::updateConfigFile();
        }

        return false;
    };
    const auto handleFileSuccess = []() -> bool
    {
        showInfoPopUp = false;
        debugUI::signalROMreset();
        updateWindowTitle();
        return true;
    };

    // On Emscripten first need to wait for indexed db to sync, then load the battery manually.
    constexpr bool AUTO_LOAD_BATTERY =
#ifdef EMSCRIPTEN
        false; 
#else
        true;
#endif

    switch (gb.loadFile(st, filePath, AUTO_LOAD_BATTERY))
    {
    case FileLoadResult::InvalidROM:      
        return handleFileError("Error Loading the ROM!");
    case FileLoadResult::InvalidBattery:   
        return handleFileError("Error Loading the Battery Save!");
    case FileLoadResult::CorruptSaveState: 
        return handleFileError("Save State is Corrupt!");
    case FileLoadResult::ROMNotFound:      
        return handleFileError("ROM Not Found! Load the ROM First.");
    case FileLoadResult::FileError:        
        return handleFileError("Error Reading the File!");
    case FileLoadResult::SuccessROM: 
    {
#ifdef EMSCRIPTEN
        const auto savesPath { gb.getSaveStateFolderPath() };
        gb.setBatterySaveFolder(savesPath);

        std::error_code err;
        if (!std::filesystem::exists(savesPath, err))
        {
            emscripten_saves_syncing = true;

            EM_ASM_ARGS ({
                var saveDir = UTF8ToString($0);
                FS.mkdir(saveDir);
                FS.mount(IDBFS, { autoPersist: true }, saveDir);

                FS.syncfs(true, function(err) {
                    if (err != null) { console.log(err); }
                    Module.ccall('emscripten_on_save_finish_sync', null,[],[]);
                });
            }, savesPath.c_str());
        }
        else
            emscripten_on_save_finish_sync();
#else
        std::ranges::fill(modifiedSaveStates, true);
#endif
        gb.gameGenies.clear();
        gb.gameSharks.clear();

        showSaveStatePopUp = false;
        return handleFileSuccess();
    }
    case FileLoadResult::SuccessSaveState:
        return handleFileSuccess();
    }

    UNREACHABLE();
}
bool loadFile(const std::filesystem::path& filePath)
{
    std::ifstream st { filePath, std::ios::in | std::ios::binary };
    const auto result = loadFile(st, filePath);
#ifdef EMSCRIPTEN
    std::error_code err;
    std::filesystem::remove(filePath, err); // Remove the file from memfs.
#endif
    return result;
}

constexpr int QUICK_SAVE_STATE = 0;

inline void loadState(int num)
{
    const auto result = num == QUICK_SAVE_STATE ? gb.loadState(gb.getSaveStateFolderPath() / "quicksave.mbs")
                                                : gb.loadState(num);

    if (result == FileLoadResult::SuccessSaveState)
    {
        debugUI::signalSaveStateChange();
        return;
    }

    if (result == FileLoadResult::FileError)
        activateInfoPopUp("Save State Doesn't Exist!");
    else
        activateInfoPopUp("Save State is Corrupt!");
}
inline void saveState(int num) 
{
    if (num == QUICK_SAVE_STATE)
        gb.saveState(gb.getSaveStateFolderPath() / "quicksave.mbs");
    else
    {
        gb.saveState(num);
        activateInfoPopUp("Save State Saved!");
    }

    modifiedSaveStates[num] = true;
}

void takeScreenshot(bool captureOpenGL)
{
#ifdef EMSCRIPTEN // Emscripten only supports RGBA format for glReadPixels
    const int CHANNELS = captureOpenGL ? 4 : 3;
    constexpr int GL_FORMAT = GL_RGBA;
#else
    constexpr int CHANNELS = 3;
    constexpr int GL_FORMAT = GL_RGB;
#endif

    std::shared_ptr<uint8_t[]> glFramebuffer;

    if (captureOpenGL)
    {
        glFramebuffer = std::make_shared<uint8_t[]>(viewport_width * viewport_height * CHANNELS);
		glReadPixels(viewport_xOffset, viewport_yOffset, viewport_width, viewport_height, GL_FORMAT, GL_UNSIGNED_BYTE, glFramebuffer.get());
    }

    const auto writePng = [=]()
    {
        void* pngBuffer = nullptr;
        size_t pngDataSize = 0;

        const int width = captureOpenGL ? viewport_width : PPU::SCR_WIDTH;
        const int height = captureOpenGL ? viewport_height : PPU::SCR_HEIGHT;
        const uint8_t* framebuffer = captureOpenGL ? glFramebuffer.get() : gb.ppu->framebufferPtr();

        pngBuffer = tdefl_write_image_to_png_file_in_memory_ex(framebuffer, width, height, CHANNELS, &pngDataSize, 3, captureOpenGL);

        if (!pngBuffer)
            return;

#ifdef EMSCRIPTEN
        const std::string fileName = gb.gameTitle + " - Screenshot.png";
        emscripten_browser_file::download(fileName.c_str(), "image/png", pngBuffer, pngDataSize);
#else
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm* now_tm = std::localtime(&now);

        std::stringstream ss;
        ss << std::put_time(now_tm, "%H-%M-%S");

        const std::string fileName = gb.gameTitle + " (" + ss.str() + ").png";
        const auto screenshotsFolder = FileUtils::executableFolderPath / "screenshots";
                
        if (!std::filesystem::exists(screenshotsFolder))
            std::filesystem::create_directory(screenshotsFolder);

        std::ofstream st(screenshotsFolder / fileName, std::ios::binary);
        st.write(reinterpret_cast<const char*>(pngBuffer), pngDataSize);
#endif
        mz_free(pngBuffer);
    };

#ifdef EMSCRIPTEN
    writePng();
#else
    if (captureOpenGL) 
        std::thread(writePng).detach();
    else // Don't create a new thread for GB screenshot, because framebuffer can be modified while writing the PNG.
        writePng();
#endif
}

void updateSelectedFilter()
{
    if (fadeEffectActive)
        currentShader->setFloat("fadeAmount", 0.0f);

    currentShader = appConfig::filter == 1 ? &lcdShader : appConfig::filter == 2 ? &scalingShader : &regularShader;

    if (currentShader->compiled())
        currentShader->use();
    else
    {
        switch (appConfig::filter)
        {
        case 1:
            lcdShader.compile(resources::lcd1xVertexShader.c_str(), resources::lcd1xFragmentShader.c_str());
            lcdShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
            break;
        case 2:
            scalingShader.compile(resources::omniscaleVertexShader.c_str(), resources::omniscaleFragmentShader.c_str());
            scalingShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
            scalingShader.setFloat2("OutputSize", PPU::SCR_WIDTH * 4, PPU::SCR_HEIGHT * 4);
            break;
        default:
            regularShader.compile(resources::regularVertexShader.c_str(), resources::regularFragmentShader.c_str());
            break;
        }
    }
}

void resetFade() 
{
    if (!fadeEffectActive)
        return;

    currentShader->setFloat("fadeAmount", 0.0f);
    fadeTime = 0.0f; 
    fadeEffectActive = false; 
}

void drawCallback(const uint8_t* framebuffer, bool firstFrame)
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuffer);

    if (firstFrame)
        OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuffer);
    else
        std::swap(gbFramebufferTextures[0], gbFramebufferTextures[1]);

    debugUI::signalVBlank();
}

void refreshGBTextures()
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gb.ppu->framebufferPtr());
    OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gb.ppu->framebufferPtr());
}

// For new palette to be applied on screen even if emulation is paused.
void refreshDMGPaletteColors(const std::array<color, 4>& newColors) 
{
    if (!gb.cartridge.ROMLoaded() || emulationRunning() || System::Current() != GBSystem::DMG)
        return;

    gb.ppu->refreshDMGScreenColors(newColors);
    refreshGBTextures();
}
void updateSelectedPalette()
{
    const auto& newColors = appConfig::palette == 0 ? PPU::BGB_GREEN_PALETTE : appConfig::palette == 1 ? PPU::GRAY_PALETTE :
                            appConfig::palette == 2 ? PPU::CLASSIC_PALETTE : PPU::CUSTOM_PALETTE;

    refreshDMGPaletteColors(newColors);
    PPU::ColorPalette = newColors.data();
}

void setOpenGL()
{
    uint32_t VAO, VBO, EBO;
    OpenGL::createQuad(VAO, VBO, EBO);

    const std::vector<uint8_t> whiteBG(PPU::FRAMEBUFFER_SIZE, 255);

    OpenGL::createTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, whiteBG.data(), appConfig::bilinearFiltering);
    OpenGL::createTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, whiteBG.data(), appConfig::bilinearFiltering);

    updateSelectedFilter();
    updateSelectedPalette(); 

    gb.setDrawCallback(drawCallback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

int getResolutionX()
{
#ifdef EMSCRIPTEN
    int screenWidth = EM_ASM_INT({
        return window.screen.width * window.devicePixelRatio;
    });
#else
    int screenWidth = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
#endif
    return screenWidth;
}
int getResolutionY()
{
#ifdef EMSCRIPTEN
    int screenHeight = EM_ASM_INT({
        return window.screen.height * window.devicePixelRatio;
    });
#else
    int screenHeight = glfwGetVideoMode(glfwGetPrimaryMonitor())->height;
#endif
    return screenHeight;
}

void updateGLViewport()
{
    viewport_xOffset = (window_width - viewport_width) / 2;
    viewport_yOffset = (window_height - menuBarHeight - viewport_height) / 2;
    glViewport(viewport_xOffset, viewport_yOffset, viewport_width, viewport_height);
}

void setIntegerScale(int newScale)
{
    if (newScale <= 0)
        return;

    int newWindowWidth = newScale * PPU::SCR_WIDTH;
    int newWindowHeight = newScale * PPU::SCR_HEIGHT + menuBarHeight;

#ifndef EMSCRIPTEN
    if (!glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) // Only resize window if its not maximized.
    {
        if (newWindowWidth <= getResolutionX() && newWindowHeight <= getResolutionY())
        {
            glfwSetWindowSize(window, newWindowWidth, newWindowHeight);
            currentIntegerScale = newScale;
        }
        return;
    }
#endif
    if (newWindowWidth > window_width || newWindowHeight > window_height)
        return;

    viewport_width = newWindowWidth;
    viewport_height = newWindowHeight - menuBarHeight;
    updateGLViewport();
    currentIntegerScale = newScale;
}

void rescaleWindow()
{
    viewport_width = window_width;
    viewport_height = window_height - menuBarHeight;

    if (appConfig::integerScaling)
    {
        currentIntegerScale = std::min(viewport_height / PPU::SCR_HEIGHT, viewport_width / PPU::SCR_WIDTH);
        viewport_width = PPU::SCR_WIDTH * currentIntegerScale;
        viewport_height = PPU::SCR_HEIGHT * currentIntegerScale;
    }
    else
    {
        constexpr float targetAspectRatio = static_cast<float>(PPU::SCR_WIDTH) / PPU::SCR_HEIGHT;
        const float windowAspectRatio = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

        if (windowAspectRatio > targetAspectRatio)
            viewport_width = viewport_height * targetAspectRatio;
        else
            viewport_height = viewport_width / targetAspectRatio;
    }

    updateGLViewport();


#ifdef EMSCRIPTEN
    emscripten_set_element_css_size (
        "canvas",
        window_width / devicePixelRatio,
        window_height / devicePixelRatio
    );
#endif
}

inline void updateImGUIViewports()
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

#ifndef EMSCRIPTEN
std::filesystem::path saveFileDialog(const std::string& defaultName, const nfdnfilteritem_t* filter)
{
    APU::IsMainThreadBlocked = true;
    fileDialogOpen = true;

    NFD::UniquePathN outPath;
    nfdresult_t result = NFD::SaveDialog(outPath, filter, 1, nullptr, FileUtils::nativePathFromUTF8(defaultName).c_str());

    APU::IsMainThreadBlocked = false;
    fileDialogOpen = false;

    return result == NFD_OKAY ? outPath.get() : std::filesystem::path();
}

std::filesystem::path openFileDialog(const nfdnfilteritem_t* filter)
{
    APU::IsMainThreadBlocked = true;
    fileDialogOpen = true;

    NFD::UniquePathN outPath;
    nfdresult_t result = NFD::OpenDialog(outPath, filter, 1);

    APU::IsMainThreadBlocked = false;
    fileDialogOpen = false;

    return result == NFD_OKAY ? outPath.get() : std::filesystem::path();
}
#else
void downloadFile(const char* filePath, const char* downloadName)
{
    std::ifstream inputFile(filePath, std::ios::binary | std::ios::ate);
    std::streampos fileSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);

    std::vector<char> buffer(fileSize);
    inputFile.read(buffer.data(), static_cast<std::streamsize>(fileSize));

    emscripten_browser_file::download(downloadName, "application/octet-stream", buffer.data(), fileSize);
}
void downloadFile(std::span<const char> buffer, const char* downloadName)
{
    emscripten_browser_file::download(downloadName, "application/octet-stream", buffer.data(), buffer.size());
}

void handle_upload_file(std::string const &filename, std::string const &mime_type, std::string_view buffer, void*)
{
    if (buffer.empty())
        return;

    memstream ms { buffer };
    loadFile(ms, filename);
}
void openFileDialog(const char* filter)
{
    emscripten_browser_file::upload(filter, handle_upload_file);
}

void emscriptenUpdateImGuiCursor()
{
#ifdef EMSCRIPTEN_DEFAULT_GLFW
    static ImGuiMouseCursor previousCursor { ImGuiMouseCursor_Arrow };
    const ImGuiMouseCursor currentCursor { ImGui::GetMouseCursor() };

    if (currentCursor != previousCursor)
    {
        const char* cssCursorName;
        switch (currentCursor)
        {
            case ImGuiMouseCursor_None:       cssCursorName = "none"; break;
            case ImGuiMouseCursor_Arrow:      cssCursorName = "default"; break;
            case ImGuiMouseCursor_TextInput:  cssCursorName = "text"; break;
            case ImGuiMouseCursor_ResizeAll:  cssCursorName = "move"; break;
            case ImGuiMouseCursor_ResizeNS:   cssCursorName = "ns-resize"; break;
            case ImGuiMouseCursor_ResizeEW:   cssCursorName = "ew-resize"; break;
            case ImGuiMouseCursor_ResizeNESW: cssCursorName = "nesw-resize"; break;
            case ImGuiMouseCursor_ResizeNWSE: cssCursorName = "nwse-resize"; break;
            case ImGuiMouseCursor_Hand:       cssCursorName = "pointer"; break;
            case ImGuiMouseCursor_NotAllowed: cssCursorName = "not-allowed"; break;
            default: cssCursorName = "default"; break;
        }

        EM_ASM_ARGS({ document.body.style.cursor = UTF8ToString($0); }, cssCursorName);
        previousCursor = currentCursor;
    }
#endif
}
#endif

bool keyConfigWindowOpen { false };

void renderKeyConfigGUI()
{
    if (ImGui::Begin("Key Configuration", &keyConfigWindowOpen, ImGuiWindowFlags_NoResize))
    {
        constexpr int totalItems = KeyBindManager::TOTAL_KEYS;
        constexpr int itemsPerColumn = (totalItems + 1) / 2;
        const float padding = ImGui::GetStyle().FramePadding.x * 2;

        const auto calculateColumnWidths = [&]()
        {
            float widths[2] = { 0.0f, 0.0f };
            for (int i = 0; i < totalItems; i++)
            {
                const char* keyName = KeyBindManager::getMegaBoyKeyName(static_cast<MegaBoyKey>(i));
                float width = ImGui::CalcTextSize(keyName).x;
                widths[i >= itemsPerColumn] = std::max(widths[i >= itemsPerColumn], width);
            }

            return std::make_pair(widths[0] + padding, widths[1] + padding);
        };

        const auto [maxWidthCol1, maxWidthCol2] = calculateColumnWidths();

        if (ImGui::BeginTable("KeyBindTable", 2, ImGuiTableFlags_NoKeepColumnsVisible | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, maxWidthCol1 + 130 * scaleFactor + padding);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, maxWidthCol2 + 130 * scaleFactor);

            const auto renderKeyBinding = [&](int index, float maxWidth)
            {
                if (index >= totalItems) return;

                ImGui::PushID(index);
                const char* keyName = KeyBindManager::getMegaBoyKeyName(static_cast<MegaBoyKey>(index));
                ImGui::Text("%s", keyName);

                float currentTextWidth = ImGui::CalcTextSize(keyName).x;
                float offsetX = maxWidth - currentTextWidth;
                ImGui::SameLine(0.0f, offsetX);

                const char* currentKey = KeyBindManager::getKeyName(KeyBindManager::keyBinds[index]);
                std::string buttonLabel = awaitingKeyBind == index ? "Press any key..." : currentKey;

                const bool yellowText = KeyBindManager::keyBinds[index] == GLFW_KEY_UNKNOWN || index == awaitingKeyBind;
                if (yellowText) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));

                if (ImGui::Button(buttonLabel.c_str(), ImVec2(130 * scaleFactor, 0)))
                    awaitingKeyBind = index;

                if (yellowText) ImGui::PopStyleColor();
                ImGui::PopID();
            };

            for (int i = 0; i < itemsPerColumn; i++)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                renderKeyBinding(i, maxWidthCol1);

                ImGui::TableSetColumnIndex(1);
                renderKeyBinding(i + itemsPerColumn, maxWidthCol2);
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const auto renderModifierSelection = []<MegaBoyKey modifier, MegaBoyKey otherModifier>()
        {
            constexpr const char* comboLabel = KeyBindManager::getMegaBoyKeyName(modifier);
            const int selectedModifier = KeyBindManager::getBind(modifier);
            const char* modifierLabel = [&]()
            {
                switch (selectedModifier)
                {
                case GLFW_MOD_SHIFT: return "Shift";
                case GLFW_MOD_CONTROL: return "Ctrl";
                case GLFW_MOD_ALT: return "Alt";
                default: return "None";
                }
            }();

            ImGui::Text("%s", comboLabel);
            ImGui::SameLine();
            ImGui::PushItemWidth(140 * scaleFactor);

            constexpr std::pair<int, const char*> modifiers[] = { { GLFW_MOD_SHIFT, "Shift" }, { GLFW_MOD_CONTROL, "Ctrl" }, { GLFW_MOD_ALT, "Alt" } };

            if (ImGui::BeginCombo((std::string("##") + comboLabel).c_str(), modifierLabel))
            {
                for (const auto& [modValue, modName] : modifiers)
                {
                    if (modValue == KeyBindManager::getBind(otherModifier)) continue;

                    const bool isSelected = selectedModifier == modValue;

                    if (ImGui::Selectable(modName, isSelected))
                    {
                        KeyBindManager::setBind(modifier, modValue);
                        appConfig::updateConfigFile();
                    }

                    if (isSelected) ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
        };

        renderModifierSelection.operator()<MegaBoyKey::SaveStateModifier, MegaBoyKey::LoadStateModifier>();
        renderModifierSelection.operator()<MegaBoyKey::LoadStateModifier, MegaBoyKey::SaveStateModifier>();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Reset to Defaults"))
        {
            KeyBindManager::keyBinds = KeyBindManager::defaultKeyBinds();
            awaitingKeyBind = -1;
            appConfig::updateConfigFile();
        }
        if (!keyConfigWindowOpen)
            awaitingKeyBind = -1;
    }
    ImGui::End();
}

bool saveStatesWindowOpen { false };

void renderSaveStatesGUI()
{
    constexpr int THUMBNAILS_PER_ROW = 3;
    static std::array<uint32_t, NUM_SAVE_STATES> saveStateTextures{};
    static int selectedSaveState = -1;

    ImGui::Begin("Save States", &saveStatesWindowOpen, ImGuiWindowFlags_AlwaysAutoResize);
    int renderedCount = 0;

    for (int i = 1; i < NUM_SAVE_STATES; i++)
    {
        const auto saveStatePath { gb.getSaveStatePath(i) };

        std::error_code err;
        if (!std::filesystem::is_regular_file(saveStatePath, err) || err)
            continue;

        if (modifiedSaveStates[i])
        {
            static std::vector<uint8_t> framebuf(PPU::FRAMEBUFFER_SIZE);

            if (!gb.loadSaveStateThumbnail(saveStatePath, framebuf))
                std::memset(framebuf.data(), 0, PPU::FRAMEBUFFER_SIZE);

            if (saveStateTextures[i])
                OpenGL::updateTexture(saveStateTextures[i], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuf.data());
            else
                OpenGL::createTexture(saveStateTextures[i], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuf.data());

            modifiedSaveStates[i] = false;
        }

        if (renderedCount % THUMBNAILS_PER_ROW != 0)
            ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::PushID(i);

        const float textWidth = ImGui::CalcTextSize(("Save " + std::to_string(i)).c_str()).x;
        const float cursorX = ImGui::GetCursorPosX() + (PPU::SCR_WIDTH - textWidth) / 2.0f;
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("Save %d", i);

        const auto imageSize { ImVec2(static_cast<int>(PPU::SCR_WIDTH * scaleFactor), static_cast<int>(PPU::SCR_HEIGHT * scaleFactor)) };

        if (gb.getSaveNum() == i)
        {
            constexpr auto lightGolden{ ImVec4(1.0f, 0.92f, 0.5f, 1.0f) };
            constexpr auto golden{ ImVec4(1.0f, 0.84f, 0.0f, 1.0f) };

            ImGui::PushStyleColor(ImGuiCol_Button, lightGolden);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, golden);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, golden);
        }

        if (ImGui::ImageButton("thumbnail", (ImTextureID)(intptr_t)saveStateTextures[i], imageSize))
        {
            selectedSaveState = i;
			showSaveStatePopUp = true;
        }

        if (gb.getSaveNum() == i)
            ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PopID();
        ImGui::EndGroup();

        renderedCount++;
    }

    if (renderedCount == 0)
        ImGui::SeparatorText("No Save States Found!");

    ImGui::End();

    if (showSaveStatePopUp && !ImGui::IsPopupOpen("Save Options"))
        ImGui::OpenPopup("Save Options");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Save Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        if (!showSaveStatePopUp)
            ImGui::CloseCurrentPopup();
        else
        {
            const auto saveStatePath { gb.getSaveStatePath(selectedSaveState) };
            const auto title { "Options for Save " + std::to_string(selectedSaveState) };

            ImGui::SeparatorText(title.c_str());
            ImGui::Spacing();

            constexpr ImVec4 buttonColor (0.2f, 0.6f, 1.0f, 0.8f);
            constexpr ImVec4 hoverColor (0.3f, 0.7f, 1.0f, 0.9f);
            constexpr ImVec4 activeColor (0.2f, 0.5f, 0.9f, 1.0f);
            constexpr ImVec4 successColor (0.4f, 0.8f, 0.4f, 0.8f);
            constexpr ImVec4 successHoverColor (0.5f, 0.9f, 0.5f, 0.9f);
            constexpr ImVec4 successActiveColor (0.3f, 0.7f, 0.3f, 1.0f);
            constexpr ImVec4 dangerColor (1.0f, 0.4f, 0.4f, 0.8f);
            constexpr ImVec4 dangerHoverColor (1.0f, 0.5f, 0.5f, 0.9f);
            constexpr ImVec4 dangerActiveColor (0.9f, 0.3f, 0.3f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

            const auto buttonSize { ImVec2(100 * scaleFactor, 0) };

            if (ImGui::Button("Load", buttonSize))
            {
                loadState(selectedSaveState);
                showSaveStatePopUp = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Copy", buttonSize))
            {
                // Instead of passing the number pass the path, so save state is just copied, without becoming the active one.
                if (gb.loadState(saveStatePath) == FileLoadResult::SuccessSaveState)
                    debugUI::signalSaveStateChange();

                showSaveStatePopUp = false;
            }

            ImGui::PopStyleColor(3);
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, successColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, successHoverColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, successActiveColor);

            if (ImGui::Button("Export", buttonSize))
            {
                const auto filename { gb.gameTitle + " - Save " + std::to_string(selectedSaveState) + ".mbs" };
#ifdef EMSCRIPTEN
                downloadFile(saveStatePath.c_str(), filename.c_str());
#else
                const auto result { saveFileDialog(filename, saveStateFilterItem) };

                if (!result.empty())
                {
                    std::error_code err;
					std::filesystem::copy(saveStatePath, result, err);			
                }
#endif
                showSaveStatePopUp = false;
            }

            ImGui::SameLine();
            ImGui::PopStyleColor(3);

            ImGui::PushStyleColor(ImGuiCol_Button, dangerColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, dangerHoverColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, dangerActiveColor);

            if (ImGui::Button("Delete", buttonSize))
            {
                if (gb.getSaveNum() == selectedSaveState)
                {
                    gb.unbindSaveState();
                    appConfig::saveStateNum = 0;
                    appConfig::updateConfigFile();
                }

                std::error_code err;
                std::filesystem::remove(saveStatePath, err);
                showSaveStatePopUp = false;
            }

            ImGui::PopStyleColor(3);
            ImGui::Spacing();

            if (ImGui::Button("Save To", buttonSize))
            {
                saveState(selectedSaveState);
                showSaveStatePopUp = false;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - buttonSize.x) * 0.5f);

            if (ImGui::Button("Cancel", buttonSize))
                showSaveStatePopUp = false;

            if (!showSaveStatePopUp)
				ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

bool cheatsWindowOpen { false };

void renderCheatsGUI() 
{
    static std::array<char, 9> sharkBuf{};
    static std::array<char, 12> genieBuf{};

    static auto isHexChar = [](char c) -> bool {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };

    ImGui::SetNextWindowSizeConstraints(ImVec2(420 * scaleFactor, 290 * scaleFactor), ImVec2(420 * scaleFactor, FLT_MAX));
    ImGui::Begin("Cheats", &cheatsWindowOpen);

    ImGui::Columns(2);
    ImGui::GetCurrentWindow()->DC.CurrentColumns->Flags |= ImGuiOldColumnFlags_NoResize;

    auto renderCheatSection = [&](const char* name, auto& buf, auto& cheats, auto validateFunc, auto addFunc)
    {
        ImGui::BeginChild(name);
        ImGui::SeparatorText(name);

        const bool isValid = validateFunc(buf);

        if (ImGui::InputText("##input", buf.data(), buf.size(), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (isValid)
				addFunc(buf, cheats);
        }

        ImGui::SameLine();

        if (!isValid) 
            ImGui::BeginDisabled();

        if (ImGui::Button("Add")) 
            addFunc(buf, cheats);

        if (!isValid) 
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) 
                ImGui::SetTooltip("Cheat Is Not Valid!");

            ImGui::EndDisabled();
        }

        ImGui::Spacing();

        const bool cheatsEmpty = cheats.empty();

        if (cheatsEmpty)
            ImGui::BeginDisabled();

        if (ImGui::Button("Clear All")) 
            cheats.clear();

        if (cheatsEmpty)
            ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::BeginChild((std::string(name) + "Cheats").c_str());

        for (int i = 0; i < cheats.size(); i++) 
        {
            auto& cheat = cheats[i];
            ImGui::PushID(i);
            ImGui::Spacing();

            ImGui::Checkbox("Enable", &cheat.enable);
            ImGui::SameLine();

            ImGui::BeginDisabled();
            ImGui::InputText("", cheat.str.data(), cheat.str.size());
            ImGui::EndDisabled();
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::EndChild();
    };

    auto validateGenie = [](const auto& buf)
    {
        return std::all_of(buf.begin(), buf.end() - 1,
            [i = -1](char ch) mutable {
                i++;
                return (i == 3 || i == 7) ? ch == '-' : isHexChar(ch);
            });
    };

    auto validateShark = [](const auto& buf) 
    {
        return std::all_of(buf.begin(), buf.end() - 1, isHexChar);
    };

    auto addGenie = [](const auto& buf, auto& cheats) 
    {
        gameGenieCheat cheat;

        cheat.newData = hexOps::fromHex(std::array { buf[0], buf[1] });
        cheat.checksum = hexOps::fromHex(buf[9]);

        cheat.oldData = hexOps::fromHex(std::array { buf[8], buf[10] });
        cheat.oldData = ((cheat.oldData >> 2) | (cheat.oldData << 6)) ^ 0xBA;

        cheat.addr = (hexOps::fromHex(buf[6]) << 12) |
                     (hexOps::fromHex(buf[2]) << 8)  |
                     (hexOps::fromHex(std::array{ buf[4], buf[5] }));

        cheat.addr ^= 0xF000;

        if (std::ranges::find(cheats, cheat) == cheats.end())
        {
            cheat.str = buf;
            cheats.push_back(cheat);
        }
    };

    auto addShark = [](const auto& buf, auto& cheats) 
    {
        gameSharkCheat cheat;

        cheat.addr = hexOps::fromHex(std::array { buf[4], buf[5], buf[6], buf[7] });
        cheat.addr = (cheat.addr << 8) | (cheat.addr >> 8);

        cheat.type = hexOps::fromHex (std::array{ buf[0], buf[1] });
        cheat.newData = hexOps::fromHex(std::array{ buf[2], buf[3] });

        if (std::ranges::find(cheats, cheat) == cheats.end())
        {
            cheat.str = buf;
            cheats.push_back(cheat);
        }
    };

    renderCheatSection("Game Genie", genieBuf, gb.gameGenies, validateGenie, addGenie);
    ImGui::NextColumn();
    renderCheatSection("Game Shark", sharkBuf, gb.gameSharks, validateShark, addShark);

    ImGui::End();
}

void renderImGUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load File"))
            {
#ifdef EMSCRIPTEN
                openFileDialog(openFilterItem);
#else
                const auto result { openFileDialog(openFilterItem) };

                if (!result.empty())
                    loadFile(result);
#endif
            }
            if (gb.cartridge.ROMLoaded())
            {
                if (ImGui::MenuItem("View Save States"))
                    saveStatesWindowOpen = true;

                if (gb.canSaveStateNow())
                {
                    if (ImGui::MenuItem("Export State"))
                    {
                        const std::string filename { gb.gameTitle + " - Save State.mbs" };
#ifdef EMSCRIPTEN
                        std::ostringstream st;
                        gb.saveState(st);
                        downloadFile(st.view(), filename.c_str());
#else
                        const auto result { saveFileDialog(filename, saveStateFilterItem) };

                        if (!result.empty())
                            gb.saveState(result);
#endif
                    }
                }
                if (gb.cartridge.hasBattery)
                {
                    if (ImGui::MenuItem("Export Battery"))
                    {
                        const std::string filename { gb.gameTitle + " - Battery Save.sav" };
#ifdef EMSCRIPTEN
                        std::ostringstream st;
                        gb.saveBattery(st);
                        downloadFile(st.view(), filename.c_str());
#else
                        const auto result { saveFileDialog(filename, batterySaveFilterItem) };

                        if (!result.empty())
                            gb.saveBattery(result);
#endif
                    }
                }
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings", "Ctrl+Q"))
        {
#ifndef EMSCRIPTEN
            if (ImGui::Checkbox("Load last ROM on Startup", &appConfig::loadLastROM))
                appConfig::updateConfigFile();
#endif
            static bool dmgBootLoaded { false };
            static bool cgbBootLoaded { false };

            if (ImGui::IsWindowAppearing())
            {
                dmgBootLoaded = GBCore::isBootROMValid(appConfig::dmgBootRomPath);
                cgbBootLoaded = GBCore::isBootROMValid(appConfig::cgbBootRomPath);
            }

            bool bootRomsLoaded = gb.cartridge.ROMLoaded() ? 
                                  (System::Current() == GBSystem::DMG ? dmgBootLoaded : cgbBootLoaded) : (dmgBootLoaded || cgbBootLoaded);

            if (!bootRomsLoaded)
            {
                const std::string tooltipText = gb.cartridge.ROMLoaded() ? 
                                                (System::Current() == GBSystem::DMG ? "Drop 'dmg_boot.bin'" : "Drop 'cgb_boot.bin'") :
                                                "Drop 'dmg_boot.bin' or 'cgb_boot.bin'";

                ImGui::BeginDisabled();
                ImGui::Checkbox("Boot ROM not Loaded!", &bootRomsLoaded);

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("%s", tooltipText.c_str());

                ImGui::EndDisabled();
            }
            else
            {
                if (ImGui::Checkbox("Run Boot ROM", &appConfig::runBootROM))
                    appConfig::updateConfigFile();
            }

            ImGui::SeparatorText("Saves");

            if (ImGui::Checkbox("Battery Saves", &appConfig::batterySaves))
                appConfig::updateConfigFile();

            if (ImGui::Checkbox("Autosave Save Slot", &appConfig::autosaveState))
                appConfig::updateConfigFile();

            ImGui::SeparatorText("Controls");

            if (ImGui::Button("Key Binding"))
                keyConfigWindowOpen = true;

            ImGui::SeparatorText("Emulated System");

            constexpr const char* preferences[] = { "Prefer GB Color", "Prefer DMG", "Force DMG" };

            if (ImGui::ListBox("##1", &appConfig::systemPreference, preferences, 3))
                appConfig::updateConfigFile();

            ImGui::EndMenu();
        }

        static bool graphicsMenuWasOpen { false };
        static bool showPaletteSelection{ false };

        if (ImGui::BeginMenu("Graphics"))
        {
            graphicsMenuWasOpen = true;

            if (emulationRunning())
                ImGui::SeparatorText(FPS_text.c_str());;

            if (lockVSyncSetting) ImGui::BeginDisabled();

            if (ImGui::Checkbox("VSync", &appConfig::vsync))
            {
                appConfig::updateConfigFile();
#ifdef EMSCRIPTEN
                const int newMode = appConfig::vsync ? EM_TIMING_RAF : EM_TIMING_SETTIMEOUT;
                const int newVal = appConfig::vsync ? 1 : 16; // Swap interval 1 with vsync, or 16 ms interval when no vsync.
                emscripten_set_main_loop_timing(newMode, newVal);
#else
                glfwSwapInterval(appConfig::vsync ? 1 : 0);
#ifdef _WIN32
                if (appConfig::vsync)
                    timeEndPeriod(1);
                else
                    timeBeginPeriod(1);
#endif
#endif
            }
            if (lockVSyncSetting)
            {
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Forced in GPU driver settings!");

                ImGui::EndDisabled();
            }

            if (ImGui::Checkbox("Screen Ghosting (Blending)", &appConfig::blending))
                appConfig::updateConfigFile();

            if (ImGui::Checkbox("Integer Scaling", &appConfig::integerScaling))
            {
                appConfig::updateConfigFile();
                rescaleWindow();
            }

            if (appConfig::integerScaling)
            {
                ImGui::SameLine();

                if (ImGui::ArrowButton("##left", ImGuiDir_Left))
                    newIntegerScale = currentIntegerScale - 1;

                ImGui::SameLine();
                ImGui::Text("X%d", currentIntegerScale);
                ImGui::SameLine();

                if (ImGui::ArrowButton("##right", ImGuiDir_Right))
                    newIntegerScale = currentIntegerScale + 1;
            }

            if (currentShader != &scalingShader)
            {
                if (ImGui::Checkbox("Bilinear Filtering", &appConfig::bilinearFiltering))
                {
                    appConfig::updateConfigFile();
                    OpenGL::setTextureScalingMode(gbFramebufferTextures[0], appConfig::bilinearFiltering);
                    OpenGL::setTextureScalingMode(gbFramebufferTextures[1], appConfig::bilinearFiltering);
                }
            }

            ImGui::SeparatorText("Filter");
            constexpr const char* filters[] = { "None", "LCD", "Upscaling" };

            const int filterCount { appConfig::bilinearFiltering ? 2 : 3 }; // Upscaling filter is disabled when bilinear filtering is enabled

            if (ImGui::ListBox("##2", &appConfig::filter, filters, filterCount))
            {
                updateSelectedFilter();
                appConfig::updateConfigFile();
            }

            ImGui::Spacing();

            if (ImGui::ArrowButton("##3", ImGuiDir_Right))
                showPaletteSelection = !showPaletteSelection;

            ImGui::SameLine();
            ImGui::SeparatorText("DMG Palette");

            if (showPaletteSelection)
            {
                constexpr std::array palettes = { "BGB Green", "Grayscale", "Classic", "Custom" };

                static bool customPaletteOpen{ false };
                static std::array<std::array<float, 3>, 4> colors{ };
                static std::array<color, 4> tempCustomPalette{ };

                const auto updateColors = []()
                {
                    for (int i = 0; i < 4; i++)
                        colors[i] = { PPU::CUSTOM_PALETTE[i].R / 255.0f, PPU::CUSTOM_PALETTE[i].G / 255.0f, PPU::CUSTOM_PALETTE[i].B / 255.0f };

                    tempCustomPalette = PPU::CUSTOM_PALETTE;
                };

                ImGui::Spacing();

                if (ImGui::ListBox("##4", &appConfig::palette, palettes.data(), palettes.size()))
                {
                    if (appConfig::palette == 3)
                    {
                        updateColors();
                        customPaletteOpen = true;
                        ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize("Custom Palette").x * 2, -1.f));
                    }
                    else
                        customPaletteOpen = false;

                    updateSelectedPalette();
                    appConfig::updateConfigFile();
                }

                if (customPaletteOpen)
                {
                    ImGui::Begin("Custom Palette", &customPaletteOpen, ImGuiWindowFlags_NoResize);

                    for (int i = 0; i < 4; i++)
                    {
                        if (ImGui::ColorEdit3(("Color " + std::to_string(i)).c_str(), colors[i].data(), ImGuiColorEditFlags_NoInputs))
                        {
                            tempCustomPalette[i] =
                            {
                                static_cast<uint8_t>(colors[i][0] * 255),
                                static_cast<uint8_t>(colors[i][1] * 255),
                                static_cast<uint8_t>(colors[i][2] * 255)
                            };

                            refreshDMGPaletteColors(tempCustomPalette);
                            PPU::CUSTOM_PALETTE[i] = tempCustomPalette[i];
                            appConfig::updateConfigFile();
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ImGui::Button("Reset to Default"))
                    {
                        refreshDMGPaletteColors(PPU::DEFAULT_CUSTOM_PALETTE);
                        PPU::CUSTOM_PALETTE = PPU::DEFAULT_CUSTOM_PALETTE;
                        updateColors();
                        appConfig::updateConfigFile();
                    }

                    ImGui::End();
                }
            }

            ImGui::EndMenu();
        }
        else
        {
            if (graphicsMenuWasOpen)
            {
                showPaletteSelection = false;
                graphicsMenuWasOpen = false;
            }
        }
        if (ImGui::BeginMenu("Audio"))
        {
            ImGui::SeparatorText("Settings");

            if (ImGui::Checkbox("Enable Audio", &appConfig::enableAudio))
                appConfig::updateConfigFile();

            static int volume { static_cast<int>(gb.apu.volume * 100) };

            ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);

            if (ImGui::SliderInt("Volume", &volume, 0, 100))
                gb.apu.volume = static_cast<float>(volume / 100.0);

            ImGui::PopItemFlag();

            ImGui::SeparatorText("Misc.");

            if (gb.apu.isRecording)
            {
                if (ImGui::Button("Stop Recording"))
                {
                    gb.apu.stopRecording();
#ifdef EMSCRIPTEN
                    downloadFile("recording.wav", "MegaBoy - Recording.wav");
                    std::error_code err;
                    std::filesystem::remove("recording.wav", err);
#endif
                }

                ImGui::SameLine();

                const auto minutes = static_cast<int>(gb.apu.recordedSeconds) / 60;
                const auto seconds = static_cast<int>(gb.apu.recordedSeconds) % 60;

                ImGui::Text("%d:%02d", minutes, seconds);
            }
            else
            {
                if (ImGui::Button("Start Recording"))
                {
#ifdef EMSCRIPTEN
                    gb.apu.startRecording("recording.wav");
#else
                    const auto result{ saveFileDialog("MegaBoy - Recording", audioSaveFilterItem) };

                    if (!result.empty())
                        gb.apu.startRecording(result);
#endif
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation"))
        {
            if (gb.cartridge.ROMLoaded())
            {
                const auto formatKeyBind = [](MegaBoyKey key) { return "(" + std::string(KeyBindManager::getKeyName(KeyBindManager::getBind(key))) + ")"; };

                const std::string pauseKeyStr = formatKeyBind(MegaBoyKey::Pause);
                const std::string resetKeyStr = formatKeyBind(MegaBoyKey::Reset);
                const std::string screenshotKeyStr = formatKeyBind(MegaBoyKey::Screenshot);

                if (ImGui::MenuItem(gb.emulationPaused ? "Resume" : "Pause", pauseKeyStr.c_str()))
                    setEmulationPaused(!gb.emulationPaused);

                if (gb.cartridge.hasBattery)
                {
                    if (ImGui::MenuItem("Reset to Battery", resetKeyStr.c_str()))
                        resetRom(false);

                    if (ImGui::MenuItem("Full Reset", "Warning!"))
                        resetRom(true);
                }
                else
                {
                    if (ImGui::MenuItem("Reset", resetKeyStr.c_str()))
                        resetRom(true);
                }

                if (ImGui::MenuItem("Take Screenshot", screenshotKeyStr.c_str()))
                    takeScreenshot(true);

                if (ImGui::MenuItem("Take 160x144 Screenshot"))
                    takeScreenshot(false);

                if (ImGui::MenuItem("Enter Cheat"))
                    cheatsWindowOpen = true;
            }

            ImGui::EndMenu();
        }

        debugUI::renderMenu();

        if (gb.breakpointHit)
        {
            ImGui::Separator();
            ImGui::Text("Breakpoint Hit!");
        }
        else if (gb.emulationPaused)
        {
            ImGui::Separator();
            ImGui::Text("Emulation Paused");
        }
        else if (fastForwarding)
        {
            ImGui::Separator();
            ImGui::Text("Fast Forward...");
        }
        else if (gb.cartridge.ROMLoaded() && gb.getSaveNum() != 0)
        {
            const std::string saveText = "Save: " + std::to_string(gb.getSaveNum());
            ImGui::Separator();
            ImGui::Text("%s", saveText.c_str());
        }

        ImGui::EndMainMenuBar();
    }

    if (showInfoPopUp && !ImGui::IsPopupOpen(popupTitle))
    {
        ImGui::OpenPopup(popupTitle);
        ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize(popupTitle).x + (ImGui::GetStyle().WindowPadding.x * 2), -1.0f), ImGuiCond_Appearing);
    }

    if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        if (!showInfoPopUp)
            ImGui::CloseCurrentPopup();
        else
        {
            ImVec2 viewportCenter = ImGui::GetMainViewport()->GetCenter();
            ImVec2 windowSize = ImGui::GetWindowSize();

            ImVec2 windowPos = ImVec2(viewportCenter.x - windowSize.x * 0.5f, viewportCenter.y - windowSize.y * 0.5f);
            ImGui::SetWindowPos(windowPos);

            const float buttonWidth = ImGui::GetContentRegionAvail().x * 0.4f;
            const float windowWidth = ImGui::GetWindowSize().x;
            ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

            if (ImGui::Button("OK", ImVec2(buttonWidth, 0)))
            {
                ImGui::CloseCurrentPopup();
                showInfoPopUp = false;
            }
        }

        ImGui::EndPopup();
    }

    if (keyConfigWindowOpen)
        renderKeyConfigGUI();

    if (saveStatesWindowOpen)
        renderSaveStatesGUI();

    if (cheatsWindowOpen)
        renderCheatsGUI();

    debugUI::renderWindows(scaleFactor);

#ifdef EMSCRIPTEN
    emscriptenUpdateImGuiCursor();
#endif

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    updateImGUIViewports();
}

void renderGameBoy(double deltaTime)
{
    OpenGL::bindTexture(gbFramebufferTextures[0]);
    currentShader->setFloat("alpha", 1.0f);

    if (fadeEffectActive)
    {
        fadeTime += static_cast<float>(deltaTime);
        const float fadeAmount = fadeTime / FADE_DURATION;

        if (fadeAmount >= 0.95f)
        {
            resetFade();
            resetRom(false);
        }
        else 
            currentShader->setFloat("fadeAmount", fadeAmount);
    }

    if (appConfig::blending)
    {
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        currentShader->setFloat("alpha", 0.5f);
        OpenGL::bindTexture(gbFramebufferTextures[1]);
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

void render(double deltaTime)
{
    glClear(GL_COLOR_BUFFER_BIT);
    renderGameBoy(deltaTime);
    renderImGUI();
    glfwSwapBuffers(window);

    if (newIntegerScale != -1)
    {
        setIntegerScale(newIntegerScale);
        newIntegerScale = -1;
    }
    if (glScreenshotRequested)
    {
        takeScreenshot(true);
        glScreenshotRequested = false;
    }
}

void key_callback(GLFWwindow* _window, int key, int scancode, int action, int mods)
{
    (void)_window; (void)scancode;

    // Don't process new key presses if ImGui is capturing keyboard input (e.g. typing in text box), but still process key releases.
    if (ImGui::GetIO().WantCaptureKeyboard && action != GLFW_RELEASE)
		return;

    if (awaitingKeyBind != -1 && key != GLFW_KEY_UNKNOWN)
    {
        if (action == GLFW_PRESS)
        {
            for (int i = 0; i < KeyBindManager::TOTAL_KEYS; i++)
            {
                if (key == KeyBindManager::keyBinds[i])
                    KeyBindManager::keyBinds[i] = GLFW_KEY_UNKNOWN;
            }

            KeyBindManager::keyBinds[awaitingKeyBind] = key;
            awaitingKeyBind = -1;
            appConfig::updateConfigFile();
        }

        return;
    }

    auto scaleUpKey = KeyBindManager::getBind(MegaBoyKey::ScaleUp);
    auto scaleDownKey = KeyBindManager::getBind(MegaBoyKey::ScaleDown);

    if (key == scaleUpKey || key == scaleDownKey)
    {
        if (appConfig::integerScaling && action == GLFW_PRESS)
            setIntegerScale(currentIntegerScale + (key == scaleUpKey ? 1 : -1));

        return;
    }

    if (!gb.cartridge.ROMLoaded()) return;

    if (action == GLFW_PRESS)
    {
        if (key == KeyBindManager::getBind(MegaBoyKey::Pause))
        {
            resetFade();
            setEmulationPaused(!gb.emulationPaused);
            return;
        }

        // number keys 1 though 9
        if (key >= 49 && key <= 57)
        {
            if (mods & KeyBindManager::getBind(MegaBoyKey::SaveStateModifier))
                saveState(key - 48);

            else if (mods & KeyBindManager::getBind(MegaBoyKey::LoadStateModifier))
                loadState(key - 48);

            return;
        }

        if (key == KeyBindManager::getBind(MegaBoyKey::QuickSave))
        {
            saveState(QUICK_SAVE_STATE);
            return;
        }
        if (key == KeyBindManager::getBind(MegaBoyKey::LoadQuickSave))
        {
            loadState(QUICK_SAVE_STATE);
            return;
        }

        if (key == KeyBindManager::getBind(MegaBoyKey::Screenshot))
        {
            // For some reason glReadPixels doesn't work from key callback on emscripten, so setting flag to take screenshot later in main loop.
            glScreenshotRequested = true;
            return;
        }
    }

    if (key == KeyBindManager::getBind(MegaBoyKey::Reset))
    {
        if (action == GLFW_PRESS)
            fadeEffectActive = true;
        else if (action == GLFW_RELEASE)
            resetFade();

        return;
    }

    if (key == KeyBindManager::getBind(MegaBoyKey::FastForward))
    {
        if (action == GLFW_PRESS)
        {
            fastForwarding = true;
            gb.enableFastForward(FAST_FORWARD_SPEED);
        }
        else if (action == GLFW_RELEASE)
        {
            fastForwarding = false;
            gb.disableFastForward();
        }

        return;
    }

    gb.joypad.update(key, action);
}

void drop_callback(GLFWwindow* _window, int count, const char** paths)
{
    (void)_window;

    if (count > 0)
        loadFile(FileUtils::nativePathFromUTF8(paths[0]));
}

bool pausedPreEvent;
void handleVisibilityChange(bool hidden)
{
    if (hidden)
    {
        pausedPreEvent = gb.emulationPaused;
        setEmulationPaused(true);
    }
    else
        setEmulationPaused(pausedPreEvent);
}

void window_iconify_callback(GLFWwindow* _window, int iconified)
{
    (void)_window;
    handleVisibilityChange(iconified);
}

#ifdef EMSCRIPTEN
EM_BOOL emscripten_resize_callback(int eventType, const EmscriptenUiEvent *uiEvent, void *userData)
{
    window_width = static_cast<int>(uiEvent->windowInnerWidth * devicePixelRatio);
    window_height = static_cast<int>(uiEvent->windowInnerHeight * devicePixelRatio);

    glfwSetWindowSize(window, window_width, window_height);
    rescaleWindow();
    render(0);

    return EM_TRUE;
}
void content_scale_callback(GLFWwindow* _window, float xScale, float yScale)
{
    devicePixelRatio = EM_ASM_DOUBLE({ return window.devicePixelRatio; });
}

const char* unloadCallback(int eventType, const void* reserved, void* userData)
{
    gb.autoSave();
    return nullptr;
}
EM_BOOL visibilityChangeCallback(int eventType, const EmscriptenVisibilityChangeEvent *visibilityChangeEvent, void *userData)
{
    handleVisibilityChange(visibilityChangeEvent->hidden);
    return EM_TRUE;
}

// Copied from imgui_impl_glfw.cpp. For some reason it never enables the callback so the wheel is not working? Also the function is static, so can't use it outside imgui.
EM_BOOL emscripten_wheel_callback(int, const EmscriptenWheelEvent* ev, void*)
{
    float multiplier = 0.0f;
    if (ev->deltaMode == DOM_DELTA_PIXEL) multiplier = 1.0f / 100.0f;        // 100 pixels make up a step.
    else if (ev->deltaMode == DOM_DELTA_LINE) { multiplier = 1.0f / 3.0f; }  // 3 lines make up a step.
    else if (ev->deltaMode == DOM_DELTA_PAGE) { multiplier = 80.0f; }        // A page makes up 80 steps.
    auto wheel_x = static_cast<float>(ev->deltaX * -multiplier);
    auto wheel_y = static_cast<float>(ev->deltaY * -multiplier);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent(wheel_x, wheel_y);
    return EM_TRUE;
}

#else
void framebuffer_size_callback(GLFWwindow* _window, int width, int height)
{
    (void)_window;
    window_width = width;
    window_height = height;
    rescaleWindow();
}
void window_pos_callback(GLFWwindow* _window, int xpos, int ypos)
{
   (void)_window; (void)xpos; (void)ypos;
   glViewport(viewport_xOffset, viewport_yOffset, viewport_width, viewport_height);
}
#endif

void window_refresh_callback(GLFWwindow* _window)
{
    if (!fileDialogOpen)
        render(0);
}

void checkVSyncStatus()
{
#if defined (__APPLE__)
    glfwSwapInterval(0);
    appConfig::vsync = false;
    lockVSyncSetting = true;
#elif defined(_WIN32) 
    auto vsyncCheckFunc = reinterpret_cast<int(*)()>(glfwGetProcAddress("wglGetSwapIntervalEXT"));

    glfwSwapInterval(0);

    if (vsyncCheckFunc())
    {
        appConfig::vsync = true;
        lockVSyncSetting = true;
    }
    else
    {
        glfwSwapInterval(1);

        if (!vsyncCheckFunc())
        {
            appConfig::vsync = false;
			lockVSyncSetting = true;
        }
        else if (!appConfig::vsync)
			glfwSwapInterval(0); // Set back to 0 if its in config.

        if (!appConfig::vsync)
			timeBeginPeriod(1);
    }
#else
    glfwSwapInterval(appConfig::vsync ? 1 : 0);
#endif
}

bool setGLFW()
{
    if (!glfwInit())
	{
		std::cout << "Failed to initialize GLFW" << std::endl;
		return false;
	}

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

#ifdef EMSCRIPTEN
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    window = glfwCreateWindow(1, 1, APP_NAME, nullptr, nullptr);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSetWindowRefreshCallback(window, window_refresh_callback);
    glfwSetDropCallback(window, drop_callback);
    glfwSetKeyCallback(window, key_callback);

#ifdef EMSCRIPTEN
    glfwSetWindowTitle(window, APP_NAME);
    glfwSetWindowContentScaleCallback(window, content_scale_callback);

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, emscripten_resize_callback);
    emscripten_set_beforeunload_callback(nullptr, unloadCallback);
    emscripten_set_visibilitychange_callback(nullptr, false, visibilityChangeCallback);
    emscripten_set_wheel_callback("canvas", nullptr, false, emscripten_wheel_callback);
#else
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowPosCallback(window, window_pos_callback);
    glfwSetWindowIconifyCallback(window, window_iconify_callback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }
#endif

    return true;
}

void setWindowSize()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::BeginMainMenuBar();
    menuBarHeight = static_cast<int>(ImGui::GetWindowSize().y);
    ImGui::EndMainMenuBar();

    ImGui::Render();
    updateImGUIViewports();

#ifdef EMSCRIPTEN
    devicePixelRatio = EM_ASM_DOUBLE ({ return window.devicePixelRatio; });

    window_width = EM_ASM_INT({ return window.innerWidth; }) * devicePixelRatio;
    window_height = EM_ASM_INT({ return window.innerHeight; }) * devicePixelRatio;

    glfwSetWindowSize(window, window_width, window_height);
    rescaleWindow();
#else
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    const int maxViewportHeight = static_cast<int>(static_cast<float>(mode->height) * 0.70f);
    const int scaleFactor = std::min(maxViewportHeight / PPU::SCR_HEIGHT, mode->width / PPU::SCR_WIDTH);

    glfwSetWindowSize(window, scaleFactor * PPU::SCR_WIDTH, (scaleFactor * PPU::SCR_HEIGHT) + menuBarHeight);
    glfwSetWindowSizeLimits(window, PPU::SCR_WIDTH, PPU::SCR_HEIGHT + menuBarHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowPos(window, (mode->width - window_width) / 2, (mode->height - window_height) / 2);
#endif
}

void setImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    scaleFactor = (static_cast<float>(getResolutionX()) / 1920.0f + static_cast<float>(getResolutionY()) / 1080.0f) / 2.0f;
    io.Fonts->AddFontFromMemoryCompressedTTF(resources::robotoMonoFont, sizeof(resources::robotoMonoFont), scaleFactor * 17.0f);
    ImGui::GetStyle().ScaleAllSizes(scaleFactor);

#ifdef __linux__
    if (std::getenv("WAYLAND_DISPLAY") == nullptr) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#elif !defined(EMSCRIPTEN)
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(GL_VERSION_STR);

#ifdef EMSCRIPTEN_DEFAULT_GLFW
    static std::string clipboardContent{};

    emscripten_browser_clipboard::paste([](std::string&& paste_data, void* callback_data [[maybe_unused]] ) {
        clipboardContent = std::move(paste_data);
    });

    ImGui::GetPlatformIO().Platform_GetClipboardTextFn = [](ImGuiContext* ctx [[maybe_unused]] ) {
        return clipboardContent.c_str();
    };

    ImGui::GetPlatformIO().Platform_SetClipboardTextFn = [](ImGuiContext* ctx [[maybe_unused]], char const* text) {
        clipboardContent = text;
        emscripten_browser_clipboard::copy(clipboardContent);
    };

    // Overriding imgui cursor pos callback, because official GLFW port has a bug, it doesn't apply hidpi scaling to cursor position and imgui doesn't handle it.
    const auto cursor_pos_callback = [](GLFWwindow* window, double xpos, double ypos) {
        ImGui_ImplGlfw_CursorPosCallback(window, xpos * devicePixelRatio, ypos * devicePixelRatio);
    };

    glfwSetCursorPosCallback(window, cursor_pos_callback);
#endif
}

void mainLoop()
{
    constexpr double MAX_DELTA_TIME = 0.1;

    static double lastFrameTime { glfwGetTime() }, lastRenderTime { lastFrameTime };
    static double gbTimer { 0.0 }, secondsTimer { 0.0 };
    static double executeTimes { 0.0 };
    static int frameCount { 0 }, gbFrameCount { 0 };

    const bool waitEvents = !emulationRunning() && !fadeEffectActive;
    const double currentTime = glfwGetTime();

    if (waitEvents)
    {
        glfwWaitEvents();
        lastFrameTime = currentTime;
    }

    const double deltaTime = std::clamp(currentTime - lastFrameTime, 0.0, MAX_DELTA_TIME);
    lastFrameTime = currentTime;

    secondsTimer += deltaTime;
    gbTimer += deltaTime;

    const bool shouldRender = appConfig::vsync || gbTimer >= GBCore::FRAME_RATE || waitEvents;

    if (shouldRender)
    {
        if (!waitEvents)
            glfwPollEvents();
    }
    else
    {
#ifndef EMSCRIPTEN
        if (!appConfig::vsync)
        {
            const double remainder = GBCore::FRAME_RATE - gbTimer;

            constexpr double SLEEP_THRESHOLD = 
#ifdef _WIN32
                0.0025; 
#else
                0.001;
#endif
            if (remainder >= SLEEP_THRESHOLD) 
            {
                // Sleep on windows is less precise than linux/macos, even with timeBeginPeriod(1). So need to sleep less time.
                std::chrono::duration<double> sleepTime (
#ifdef _WIN32
                    remainder <= 0.004 ? 0.001 : remainder <= 0.006 ? 0.002 : remainder / 1.6
#else
                    remainder / 1.5
#endif
                );
                std::this_thread::sleep_for(sleepTime);
            }
        }
#endif
    }
    while (gbTimer >= GBCore::FRAME_RATE)
    {
        if (emulationRunning())
		{
            const auto execStart = glfwGetTime();
            APU::LastMainThreadTime = execStart;

            gb.emulateFrame();

            executeTimes += (glfwGetTime() - execStart);
            gbFrameCount++;
		}

        gbTimer -= GBCore::FRAME_RATE;
    }

    if (shouldRender)
    {
        render(currentTime - lastRenderTime);
        lastRenderTime = currentTime;
        frameCount++;
    }

    if (secondsTimer >= 1.0)
    {
        if (emulationRunning())
        {
            const double avgExecuteTime = (executeTimes / gbFrameCount) * 1000;
            const double fps = frameCount / secondsTimer;

            std::ostringstream oss;
            oss << "FPS: " << std::fixed << std::setprecision(2) << fps << " - " << avgExecuteTime << " ms";
            FPS_text = oss.str();

#ifndef EMSCRIPTEN
            updateWindowTitle();
#endif
            gb.autoSave();

            if (appConfig::autosaveState && gb.getSaveNum() != 0)
                modifiedSaveStates[gb.getSaveNum()] = true;
        }

        gbFrameCount = 0;
        frameCount = 0;
        executeTimes = 0;
        secondsTimer = 0;
    }
}

#ifdef EMSCRIPTEN
extern "C" EMSCRIPTEN_KEEPALIVE void runApp()
#else
void runApp(int argc, char* argv[])
#endif
{
    appConfig::loadConfigFile();

    setGLFW();
    setOpenGL();
    setImGUI();
    setWindowSize();

#ifndef EMSCRIPTEN
    checkVSyncStatus();
    NFD_Init();

    if (argc > 1)
    {
#ifdef _WIN32
        auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
#endif
        loadFile(argv[1]);
}
    else
    {
        if (appConfig::loadLastROM && !appConfig::romPath.empty())
        {
            if (loadFile(appConfig::romPath))
            {
                if (appConfig::saveStateNum != 0)
                    loadState(appConfig::saveStateNum);
            }
        }
    }

    while (!glfwWindowShouldClose(window)) { mainLoop(); }
#else
    emscripten_set_main_loop(mainLoop, appConfig::vsync ? 0 : 60, false);
#endif
}

int main(int argc, char* argv[])
{
#ifdef EMSCRIPTEN
    EM_ASM ({
        FS.mkdir("/saves");
        FS.mkdir('/data');
        FS.mount(IDBFS, { autoPersist: true }, '/data');

        FS.syncfs(true, function(err) {
            if (err != null) { console.log(err); }
            Module.ccall('runApp', null, [], []);
        });
    });
#else
    runApp(argc, argv);
    gb.autoSave();

    NFD_Quit();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    glfwTerminate();
#endif

    return 0;
}