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
#include <ImGUI/imgui_impl_glfw.h>
#include <ImGUI/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <miniz/miniz.h>

#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <emscripten_browser_file/emscripten_browser_file.h>
#else
#include <glad/glad.h>
#include <nfd.hpp>
#include <thread>
#endif

static_assert(std::endian::native == std::endian::little, "This program requires a little-endian architecture.");
static_assert(CHAR_BIT == 8, "This program requires 'char' to be 8 bits in size.");

GLFWwindow* window;

int menuBarHeight{};
int window_width{}, window_height{};
int viewport_width{}, viewport_height{};
int viewport_xOffset{}, viewport_yOffset{};
float scaleFactor{};

int currentIntegerScale{};
int newIntegerScale { -1 };

#ifdef EMSCRIPTEN
double devicePixelRatio{};
#endif

Shader regularShader;
Shader scalingShader;
Shader lcdShader;
std::array<uint32_t, 2> gbFramebufferTextures {};

Shader* currentShader{ };

bool glScreenshotRequested { false };
bool fileDialogOpen { false };

#ifndef EMSCRIPTEN
constexpr nfdnfilteritem_t openFilterItem[] = { { N_STR("Game ROM/Save"), N_STR("gb,gbc,zip,sav,mbs,bin") } };
constexpr nfdnfilteritem_t saveStateFilterItem[] = { { N_STR("Save State"), N_STR("mbs") } };
constexpr nfdnfilteritem_t batterySaveFilterItem[] = { { N_STR("Battery Save"), N_STR("sav") } };
constexpr nfdnfilteritem_t audioSaveFilterItem[] = { { N_STR("WAV File"), N_STR("wav") } };
#else
constexpr const char* openFilterItem = ".gb,.gbc,.zip,.sav,.mbs,.bin";
#endif

const char* popupTitle = "";
bool showPopUp { false };

std::string FPS_text{ "FPS: 00.00 - 0.00 ms" };

extern GBCore gbCore;

constexpr float FADE_DURATION = 0.9f;
bool fadeEffectActive { false };
float fadeTime { 0.0f };

bool fastForwarding { false };
constexpr int FAST_FORWARD_SPEED = 5;

bool lockVSyncSetting { false };

int awaitingKeyBind { -1 };

inline void updateWindowTitle()
{
    std::string title = (gbCore.gameTitle.empty() ? "MegaBoy" : "MegaBoy - " + gbCore.gameTitle);
#ifndef EMSCRIPTEN
    if (gbCore.cartridge.ROMLoaded()) title += !gbCore.emulationPaused ? (" (" + FPS_text + ")") : "";
#endif
    glfwSetWindowTitle(window, title.c_str());
}

inline void setEmulationPaused(bool val)
{
    gbCore.emulationPaused = val;
    updateWindowTitle();
}

#ifdef EMSCRIPTEN
bool emscripten_saves_syncing { false };
extern "C" EMSCRIPTEN_KEEPALIVE void emscripten_on_save_finish_sync()
{
    gbCore.loadCurrentBatterySave();
    emscripten_saves_syncing = false;
}
#endif

inline bool emulationRunning()
{
    const bool isRunning = !gbCore.emulationPaused && gbCore.cartridge.ROMLoaded() && !gbCore.breakpointHit;

#ifdef EMSCRIPTEN
    return isRunning && !emscripten_saves_syncing;
#else
    return isRunning;
#endif
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
        popupTitle = "Successfully Loaded Boot ROM!";
    }
    else
        popupTitle = "Invalid Boot ROM!";

    showPopUp = true;
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

    const auto handleFileError = [&](const char* errorMessage) -> bool
    {
        popupTitle = errorMessage;
        showPopUp = true;

        if (!gbCore.cartridge.ROMLoaded())
        {
            appConfig::romPath.clear();
            appConfig::updateConfigFile();
        }

        return false;
    };
    const auto handleFileSuccess = [&]() -> bool
    {
        showPopUp = false;
        debugUI::signalROMLoaded();
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

    switch (gbCore.loadFile(st, filePath, AUTO_LOAD_BATTERY))
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
        const auto savePath = gbCore.getSaveStateFolderPath();
        gbCore.setBatterySaveFolder(savePath);
        emscripten_saves_syncing = true;

        EM_ASM_ARGS ({
            var saveDir = UTF8ToString($0);
            FS.mkdir(saveDir);
            FS.mount(IDBFS, { autoPersist: true }, saveDir);

            FS.syncfs(true, function(err) {
                if (err != null) { console.log(err); }
                Module.ccall('emscripten_on_save_finish_sync', null, [], []);
            });
        }, savePath.string().c_str());
#endif
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
    const auto result = num == QUICK_SAVE_STATE ? gbCore.loadState(gbCore.getSaveStateFolderPath() / "quicksave.mbs")
                                                : gbCore.loadState(num);

    if (result == FileLoadResult::SuccessSaveState)
        return;

    if (result == FileLoadResult::FileError)
    {
        popupTitle = "Save State Doesn't Exist!";
        showPopUp = true;
    }
    else
	{
		popupTitle = "Save State is Corrupt!";
		showPopUp = true;
	}
}
inline void saveState(int num) 
{
    if (num == QUICK_SAVE_STATE)
        gbCore.saveState(gbCore.getSaveStateFolderPath() / "quicksave.mbs");
    else
    {
        gbCore.saveState(num);
        showPopUp = true;
        popupTitle = "Save State Saved!";
    }
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
        const uint8_t* framebuffer = captureOpenGL ? glFramebuffer.get() : gbCore.ppu->framebufferPtr();

        pngBuffer = tdefl_write_image_to_png_file_in_memory_ex(framebuffer, width, height, CHANNELS, &pngDataSize, 3, captureOpenGL);

        if (!pngBuffer)
            return;

#ifdef EMSCRIPTEN
        const std::string fileName = gbCore.gameTitle + " - Screenshot.png";
        emscripten_browser_file::download(fileName.c_str(), "image/png", pngBuffer, pngDataSize);
#else
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm* now_tm = std::localtime(&now);

        std::stringstream ss;
        ss << std::put_time(now_tm, "%H-%M-%S");

        const std::string fileName = gbCore.gameTitle + " (" + ss.str() + ").png";
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
            scalingShader.setFloat2("OutputSize", PPU::SCR_WIDTH * 4, PPU::SCR_HEIGHT * 4);
            scalingShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
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
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->framebufferPtr());
    OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->framebufferPtr());
}

// So new palette is applied on screen even if emulation is paused.
void refreshDMGPaletteColors(const std::array<color, 4>& newColors) 
{
    if (!gbCore.emulationPaused || System::Current() != GBSystem::DMG)
        return;

    gbCore.ppu->refreshDMGScreenColors(newColors);
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

    gbCore.setDrawCallback(drawCallback);

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
std::optional<std::filesystem::path> saveFileDialog(const std::string& defaultName, const nfdnfilteritem_t* filter)
{
    fileDialogOpen = true;
    NFD::UniquePathN outPath;
    nfdresult_t result = NFD::SaveDialog(outPath, filter, 1, nullptr, FileUtils::nativePathFromUTF8(defaultName).c_str());

    fileDialogOpen = false;
    return result == NFD_OKAY ? std::make_optional(outPath.get()) : std::nullopt;
}

std::optional<std::filesystem::path> openFileDialog(const nfdnfilteritem_t* filter)
{
    fileDialogOpen = true;
    NFD::UniquePathN outPath;
    nfdresult_t result = NFD::OpenDialog(outPath, filter, 1);

    fileDialogOpen = false;
    return result == NFD_OKAY ? std::make_optional(outPath.get()) : std::nullopt;
}
#else
void downloadFile(const char* fileName)
{
    std::ifstream inputFile(fileName, std::ios::binary | std::ios::ate);
    std::streampos fileSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);

    std::vector<char> buffer(fileSize);
    inputFile.read(buffer.data(), static_cast<std::streamsize>(fileSize));

    emscripten_browser_file::download(fileName, "application/octet-stream", buffer.data(), fileSize);
    std::filesystem::remove(fileName);
}
void downloadFile(std::span<const char> buffer, const char* fileName)
{
    emscripten_browser_file::download(fileName, "application/octet-stream", buffer.data(), buffer.size());
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
                        KeyBindManager::setBind(modifier, modValue);

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
bool updateSaveStatesGUI { true };

void renderSaveStatesGUI()
{
    constexpr int NUM_SAVE_STATES = 10;
    constexpr int THUMBNAILS_PER_ROW = 3;

    static std::array<uint32_t, NUM_SAVE_STATES> saveStateTextures{};
    static std::vector<uint8_t> tempGbFramebuf(PPU::FRAMEBUFFER_SIZE);

    static int selectedSaveState = -1; 

    ImGui::Begin("Save States", &saveStatesWindowOpen, ImGuiWindowFlags_AlwaysAutoResize);

    int renderedCount = 0; 

    for (int i = 1; i < NUM_SAVE_STATES; i++)
    {
        const auto saveStatePath = gbCore.getSaveStateFolderPath() / ("save" + std::to_string(i) + ".mbs");

        std::error_code err;
        if (!std::filesystem::exists(saveStatePath, err))
            continue;

        if (updateSaveStatesGUI)
        {
            if (!gbCore.loadSaveStateThumbnail(saveStatePath, tempGbFramebuf.data()))
                continue;

            if (saveStateTextures[i])
                OpenGL::updateTexture(saveStateTextures[i], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, tempGbFramebuf.data());
            else
                OpenGL::createTexture(saveStateTextures[i], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, tempGbFramebuf.data());
        }

        if (renderedCount % THUMBNAILS_PER_ROW != 0)
            ImGui::SameLine();

        ImGui::BeginGroup();

        const float textWidth = ImGui::CalcTextSize(("Save " + std::to_string(i)).c_str()).x;
        const float cursorX = ImGui::GetCursorPosX() + (PPU::SCR_WIDTH - textWidth) / 2.0f;
        ImGui::SetCursorPosX(cursorX);
        ImGui::Text("Save %d", i);

        ImGui::Image(reinterpret_cast<void*>(saveStateTextures[i]), ImVec2(PPU::SCR_WIDTH, PPU::SCR_HEIGHT));

        ImGui::Spacing();

        const float buttonWidth = PPU::SCR_WIDTH * 0.5f;
        const float buttonX = ImGui::GetCursorPosX() + (PPU::SCR_WIDTH - buttonWidth) / 2.0f;
        ImGui::SetCursorPosX(buttonX);

        if (ImGui::Button(("Options##" + std::to_string(i)).c_str(), ImVec2(buttonWidth, 0)))
            selectedSaveState = i;

        ImGui::EndGroup();
        renderedCount++; 
    }

    ImGui::End();

    if (selectedSaveState != -1)
    {
        const auto saveStatePath = gbCore.getSaveStateFolderPath() / ("save" + std::to_string(selectedSaveState) + ".mbs");
        ImGui::Begin("Save Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        const std::string title = "Options for Save " + std::to_string(selectedSaveState);
        ImGui::SeparatorText(title.c_str());
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));       
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.35f, 0.75f, 1.0f)); 

        if (ImGui::Button("Load", ImVec2(100, 0))) 
        {
            loadState(selectedSaveState);
            selectedSaveState = -1; 
        }

        ImGui::SameLine();

        if (ImGui::Button("Save To", ImVec2(100, 0))) 
        {
            saveState(selectedSaveState);
            selectedSaveState = -1;
        }

        ImGui::PopStyleColor(3);
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));     
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.7f, 0.15f, 1.0f)); 

        if (ImGui::Button("Export", ImVec2(100, 0)))
        {
            // export
            selectedSaveState = -1; 
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));       
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.15f, 0.15f, 1.0f)); 

        if (ImGui::Button("Delete", ImVec2(100, 0)))
        {
            std::filesystem::remove(saveStatePath);
            selectedSaveState = -1; 
        }
        ImGui::PopStyleColor(6);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing(); 

        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            selectedSaveState = -1;

        ImGui::End();  
    }

    if (updateSaveStatesGUI)
        updateSaveStatesGUI = false;
}

void renderImGUI()
{
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
                auto result = openFileDialog(openFilterItem);

                if (result.has_value())
                    loadFile(result.value());
#endif
            }
            if (gbCore.cartridge.ROMLoaded())
            {
                if (ImGui::MenuItem("View Save States"))
                    saveStatesWindowOpen = true;

                if (gbCore.canSaveStateNow())
                {
                    if (ImGui::MenuItem("Export State"))
                    {
                        const std::string filename = gbCore.gameTitle + " - Save State.mbs";
#ifdef EMSCRIPTEN
                        std::ostringstream st;
                        gbCore.saveState(st);
                        downloadFile(st.view(), filename.c_str());
#else
                        auto result = saveFileDialog(filename, saveStateFilterItem);

                        if (result.has_value())
                            gbCore.saveState(result.value());
#endif
                    }
                }
                if (gbCore.cartridge.hasBattery)
                {
                    if (ImGui::MenuItem("Export Battery"))
                    {
                        const std::string filename = gbCore.gameTitle + " - Battery Save.sav";
#ifdef EMSCRIPTEN
                        std::ostringstream st;
                        gbCore.saveBattery(st);
                        downloadFile(st.view(), filename.c_str());
#else
                        auto result = saveFileDialog(filename, batterySaveFilterItem);

                        if (result.has_value())
                            gbCore.saveBattery(result.value());
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

            bool bootRomsLoaded = gbCore.cartridge.ROMLoaded() ? 
                                 (System::Current() == GBSystem::DMG ? dmgBootLoaded : cgbBootLoaded) : (dmgBootLoaded || cgbBootLoaded);

            if (!bootRomsLoaded)
            {
                const std::string tooltipText = gbCore.cartridge.ROMLoaded() ? 
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
                keyConfigWindowOpen = !keyConfigWindowOpen;

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
                constexpr std::array<const char*, 4> palettes = { "BGB Green", "Grayscale", "Classic", "Custom" };

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

            static int volume { static_cast<int>(gbCore.apu.volume * 100) };

            if (ImGui::SliderInt("Volume", &volume, 0, 100))
                gbCore.apu.volume = static_cast<float>(volume / 100.0);

            ImGui::SeparatorText("Misc.");

            if (ImGui::Button(gbCore.apu.recording ? "Stop Recording" : "Start Recording"))
            {
                if (gbCore.apu.recording)
                {
                    gbCore.apu.stopRecording();
#ifdef EMSCRIPTEN
                    downloadFile("Recording.wav");
#endif
                }
                else
                {
#ifdef EMSCRIPTEN
                    gbCore.apu.startRecording("Recording.wav"); // TODO: verify if works properly.
#else
                    auto result = saveFileDialog("Recording", audioSaveFilterItem);

                    if (result.has_value())
                        gbCore.apu.startRecording(result.value());
#endif
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation"))
        {
            if (gbCore.cartridge.ROMLoaded())
            {
                const auto formatKeyBind = [](MegaBoyKey key) { return "(" + std::string(KeyBindManager::getKeyName(KeyBindManager::getBind(key))) + ")"; };

                const std::string pauseKeyStr = formatKeyBind(MegaBoyKey::Pause);
                const std::string resetKeyStr = formatKeyBind(MegaBoyKey::Reset);
                const std::string screenshotKeyStr = formatKeyBind(MegaBoyKey::Screenshot);

                if (ImGui::MenuItem(gbCore.emulationPaused ? "Resume" : "Pause", pauseKeyStr.c_str()))
                    setEmulationPaused(!gbCore.emulationPaused);

                if (gbCore.cartridge.hasBattery)
                {
                    if (ImGui::MenuItem("Reset to Battery", resetKeyStr.c_str()))
                        gbCore.resetRom(false);

                    if (ImGui::MenuItem("Full Reset", "Warning!"))
                        gbCore.resetRom(true);
                }
                else
                {
                    if (ImGui::MenuItem("Reset", resetKeyStr.c_str()))
                        gbCore.resetRom(true);
                }

                if (ImGui::MenuItem("Take Screenshot", screenshotKeyStr.c_str()))
                    takeScreenshot(true);

                if (ImGui::MenuItem("Take 160x144 Screenshot"))
                    takeScreenshot(false);
            }

            ImGui::EndMenu();
        }

        debugUI::updateMenu();

        if (gbCore.breakpointHit)
        {
            ImGui::Separator();
			ImGui::Text("Breakpoint Hit!");
        }
        else if (gbCore.emulationPaused)
        {
            ImGui::Separator();
            ImGui::Text("Emulation Paused");
        }
        else if (fastForwarding)
        {
			ImGui::Separator();
			ImGui::Text("Fast Forward...");
        }
        else if (gbCore.cartridge.ROMLoaded() && gbCore.getSaveNum() != 0)
        {
            const std::string saveText = "Save: " + std::to_string(gbCore.getSaveNum());
            ImGui::Separator();
            ImGui::Text("%s", saveText.c_str());
        }

        ImGui::EndMainMenuBar();
    }

    if (showPopUp && !ImGui::IsPopupOpen(popupTitle))
    {
        ImGui::OpenPopup(popupTitle);
        ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize(popupTitle).x + (ImGui::GetStyle().WindowPadding.x * 2), -1.0f), ImGuiCond_Appearing);
    }

    if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        if (!showPopUp)
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
                showPopUp = false;
            }
        }

        ImGui::EndPopup();
    }

    if (keyConfigWindowOpen)
        renderKeyConfigGUI();

    if (saveStatesWindowOpen)
        renderSaveStatesGUI();

    debugUI::updateWindows(scaleFactor);

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
            gbCore.resetRom(false);
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

    if (!gbCore.cartridge.ROMLoaded()) return;

    if (action == GLFW_PRESS)
    {
        if (key == KeyBindManager::getBind(MegaBoyKey::Pause))
        {
            resetFade();
            setEmulationPaused(!gbCore.emulationPaused);
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
            gbCore.enableFastForward(FAST_FORWARD_SPEED);
        }
        else if (action == GLFW_RELEASE)
        {
            fastForwarding = false;
            gbCore.disableFastForward();
        }

        return;
    }

    gbCore.input.update(key, action);
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
        pausedPreEvent = gbCore.emulationPaused;
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
    glfwSwapBuffers(window);

    return EM_TRUE;
}
void content_scale_callback(GLFWwindow* _window, float xScale, float yScale)
{
    devicePixelRatio = EM_ASM_DOUBLE({ return window.devicePixelRatio; });
}
EM_BOOL visibilityChangeCallback(int eventType, const EmscriptenVisibilityChangeEvent *visibilityChangeEvent, void *userData)
{
    handleVisibilityChange(visibilityChangeEvent->hidden);
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

    window = glfwCreateWindow(1, 1, "MegaBoy", nullptr, nullptr);
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

#ifdef  EMSCRIPTEN
    glfwSetWindowTitle(window, "MegaBoy");

    glfwSetWindowContentScaleCallback(window, content_scale_callback);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, emscripten_resize_callback);
    emscripten_set_visibilitychange_callback(nullptr, false, visibilityChangeCallback);
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
}

void mainLoop()
{
    constexpr double MAX_DELTA_TIME = 0.1;

    static double lastFrameTime{ glfwGetTime() };
    static double lastRenderTime { lastFrameTime };
    static double gbTimer { 0.0 };
    static double secondsTimer { 0.0 };
    static double executeTimes { 0.0 };
    static int frameCount{ 0 };
    static int gbFrameCount{ 0 };

    const bool waitEvents = (gbCore.emulationPaused && !fadeEffectActive) || !gbCore.cartridge.ROMLoaded();

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
        glfwPollEvents();
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
            gbCore.update(GBCore::CYCLES_PER_FRAME);

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
            gbCore.autoSave();
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
            const int saveNum = appConfig::saveStateNum;

            if (loadFile(appConfig::romPath))
            {
                if (saveNum >= 1 && saveNum <= 9)
                    loadState(saveNum);
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
    gbCore.autoSave();

    NFD_Quit();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    glfwTerminate();
#endif

    return 0;
}