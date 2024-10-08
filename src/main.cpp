#include <ImGUI/imgui.h>
#include <ImGUI/imgui_impl_glfw.h>
#include <ImGUI/imgui_impl_opengl3.h>

#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <emscripten_browser_file/emscripten_browser_file.h>
#else
#include <glad/glad.h>
#include <nfd.hpp>
#endif

#include <GLFW/glfw3.h>

#include <thread>
#include <iostream>
#include <filesystem>
#include <optional>

#include "GBCore.h"
#include "gbSystem.h"
#include "appConfig.h"
#include "debugUI.h"
#include "resources.h"
#include "Utils/Shader.h"
#include "Utils/fileUtils.h"
#include "Utils/glFunctions.h"

GLFWwindow* window;

int menuBarHeight;
int viewport_width, viewport_height;
float scaleFactor;

Shader regularShader;
Shader scalingShader;
Shader lcdShader;
std::array<uint32_t, 2> gbFramebufferTextures;

Shader* currentShader;

bool fileDialogOpen;

#ifndef EMSCRIPTEN
#ifdef _WIN32
#define STR(s) L##s
#else
#define STR(s) s
#endif

constexpr nfdnfilteritem_t openFilterItem[] = { {STR("Game ROM/Save"), STR("gb,gbc,sav,mbs,bin")} };
constexpr nfdnfilteritem_t saveStateFilterItem[] = { {STR("Save State"), STR("mbs")} };
constexpr nfdnfilteritem_t batterySaveFilterItem[] = { {STR("Battery Save"), STR("sav")} };
constexpr nfdnfilteritem_t audioSaveFilterItem[] = { {STR("WAV File"), STR("wav")} };

#undef STR
#else
constexpr const char* openFilterItem = ".gb,.gbc,.sav,.mbs,.bin";
#endif

const char* popupTitle = "";
bool showPopUp{false};

std::string FPS_text{ "FPS: 00.00 - 0.00 ms" };

extern GBCore gbCore;

double lastFrameTime = glfwGetTime();
double fpsTimer{};
double timer{};
int frameCount{};
double frameTimes{};

bool lockVSyncSetting { false };

inline void updateWindowTitle()
{
    std::string title = (gbCore.gameTitle.empty() ? "MegaBoy" : "MegaBoy - " + gbCore.gameTitle);
#ifndef EMSCRIPTEN
    if (gbCore.cartridge.ROMLoaded) title += !gbCore.emulationPaused ? (" (" + FPS_text + ")") : "";
#endif
    glfwSetWindowTitle(window, title.c_str());
}

inline void setEmulationPaused(bool val)
{
    gbCore.emulationPaused = val;
    if (!val) lastFrameTime = glfwGetTime();
    updateWindowTitle();
}

constexpr const char* DMG_ROM_NAME = "dmg_boot.bin";
constexpr const char* CGB_ROM_NAME = "cgb_boot.bin";

void handleBootROMLoad(std::string& destRomPath, const std::filesystem::path& filePath)
{
    destRomPath = filePath.string();
    appConfig::updateConfigFile();

    popupTitle = "Successfully Loaded Boot ROM!";
    showPopUp = true;
}

std::filesystem::path currentFilePath{};
inline bool loadFile(const std::filesystem::path& path)
{
    if (std::ifstream ifs { path })
    {
        ifs.close();

        if (path.filename() == DMG_ROM_NAME)
        {
            handleBootROMLoad(appConfig::dmgRomPath, path);
            return true;
        }
        if (path.filename() == CGB_ROM_NAME)
        {
            handleBootROMLoad(appConfig::cgbRomPath, path);
            return true;
        }

        switch (gbCore.loadFile(path))
        {
        case FileLoadResult::InvalidROM:
            popupTitle = "Error Loading the ROM!";
            showPopUp = true;
            break;
        case FileLoadResult::SaveStateROMNotFound:
            popupTitle = "ROM not found! Load the ROM first.";
            showPopUp = true;
            break;
        case FileLoadResult::Success:
        {
#ifdef EMSCRIPTEN
            if (path != currentFilePath && currentFilePath.filename() != DMG_ROM_NAME && currentFilePath.filename() != CGB_ROM_NAME)
                std::filesystem::remove(currentFilePath);
#endif
            updateWindowTitle();
            currentFilePath = path;
            return true;
        }
        }
    }

#ifdef EMSCRIPTEN
    std::filesystem::remove(path);
#endif

    return false;
}

void updateSelectedFilter()
{
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

void drawCallback(const uint8_t* framebuffer)
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuffer);
    std::swap(gbFramebufferTextures[0], gbFramebufferTextures[1]);
}

void refreshGBTextures()
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->getFrameBuffer());
    OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->getFrameBuffer());
}

void updateSelectedPalette()
{
    const auto& newColors = appConfig::palette == 0 ? PPU::BGB_GREEN_PALETTE : appConfig::palette == 1 ? PPU::GRAY_PALETTE : PPU::CLASSIC_PALETTE;

    if (gbCore.emulationPaused && System::Current() == GBSystem::DMG)
    {
        gbCore.ppu->refreshDMGScreenColors(newColors);
        refreshGBTextures();
    }

    PPU::ColorPalette = newColors;
}

void setOpenGL()
{
    unsigned int VAO, VBO, EBO;

    constexpr unsigned int indices[] =
    {
        0, 1, 3,
        1, 2, 3
    };
    constexpr float vertices[] =
    {
        1.0f,  1.0f, 0.0f,  1.0f,  0.0f,  // top right     
        1.0f, -1.0f, 0.0f,  1.0f,  1.0f,  // bottom right
       -1.0f, -1.0f, 0.0f,  0.0f,  1.0f,  // bottom left
       -1.0f,  1.0f, 0.0f,  0.0f,  0.0f   // top left 
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const std::vector<uint8_t> whiteBG(PPU::FRAMEBUFFER_SIZE, 255);

    OpenGL::createTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, whiteBG.data());
    OpenGL::createTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, whiteBG.data());

    updateSelectedFilter();
    updateSelectedPalette(); 

    gbCore.setDrawCallback(drawCallback);
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

inline bool isBootROMFile(const std::string& fileName) { return fileName == "dmg_boot.bin" || fileName == "cgb_boot.bin"; }

#ifndef EMSCRIPTEN
std::optional<std::filesystem::path> saveFileDialog(const std::string& defaultName, const nfdnfilteritem_t* filter)
{
    fileDialogOpen = true;
    NFD::UniquePathN outPath;

    const auto NdefaultName = FileUtils::nativePath(defaultName);
    nfdresult_t result = NFD::SaveDialog(outPath, filter, 1, nullptr, NdefaultName.c_str());

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
    inputFile.read(buffer.data(), fileSize);

    emscripten_browser_file::download(fileName, "application/octet-stream", buffer.data(), fileSize);
    std::filesystem::remove(fileName);
}

void handle_upload_file(std::string const &filename, std::string const &mime_type, std::string_view buffer, void*)
{
    std::ofstream fs(filename, std::ios::out | std::ios::binary);
    fs.write(buffer.data(), buffer.size());
    fs.close();

    loadFile(filename);
}
void openFileDialog(const char* filter)
{
    emscripten_browser_file::upload(filter, handle_upload_file);
}

void emscripten_download_saveState()
{
    const std::string fileName = gbCore.gameTitle + " - Save State.mbs";
    gbCore.saveState(fileName);
    downloadFile(fileName.c_str());
}
void emscripten_download_batterySave()
{
    const std::string fileName = gbCore.gameTitle + " - Battery Save.sav";
    gbCore.saveBattery(fileName);
    downloadFile(fileName.c_str());
}
#endif

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

            if (!currentFilePath.empty())
            {
                if (ImGui::MenuItem("Reload"))
                    loadFile(currentFilePath);
            }

            if (gbCore.cartridge.ROMLoaded)
            {
                if (ImGui::MenuItem("Export State"))
                {
#ifdef EMSCRIPTEN
                    emscripten_download_saveState();
#else
                    auto result = saveFileDialog(gbCore.gameTitle + " - Save State", saveStateFilterItem);

                    if (result.has_value())
                        gbCore.saveState(result.value());
#endif
                }

                if (gbCore.cartridge.hasBattery)
                {
                    if (ImGui::MenuItem("Export Battery"))
                    {
#ifdef EMSCRIPTEN
                        emscripten_download_batterySave();
#else
                        auto result = saveFileDialog(gbCore.gameTitle + " - Battery Save", batterySaveFilterItem);

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
            static bool romsExist{ false };

            if (ImGui::IsWindowAppearing())
                romsExist = std::filesystem::exists(FileUtils::nativePath(appConfig::dmgRomPath)) || std::filesystem::exists(FileUtils::nativePath(appConfig::cgbRomPath));

            if (!romsExist)
            {
                ImGui::BeginDisabled();
                ImGui::Checkbox("Boot ROMs not Loaded!", &romsExist);

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Drop 'dmg_boot.bin' or 'cgb_boot.bin'");

                ImGui::EndDisabled();
            }
            else
            {
                if (ImGui::Checkbox("Run Boot ROM", &appConfig::runBootROM))
                    appConfig::updateConfigFile();
            }

            if (ImGui::Checkbox("Pause when unfocused", &appConfig::pauseOnFocus))
                appConfig::updateConfigFile();

            ImGui::SeparatorText("Saves");

            if (ImGui::Checkbox("Battery Saves", &appConfig::batterySaves))
                appConfig::updateConfigFile();

            if (ImGui::Checkbox("Autosave Save State", &appConfig::autosaveState))
                appConfig::updateConfigFile();

#ifndef EMSCRIPTEN
            if (ImGui::Checkbox("Create Backups", &appConfig::backupSaves))
                appConfig::updateConfigFile();
#endif

            ImGui::SeparatorText("Emulated System");

            constexpr const char* preferences[] = { "Prefer GB Color", "Prefer DMG", "Force DMG" };

            if (ImGui::ListBox("##1", &appConfig::systemPreference, preferences, 3))
                appConfig::updateConfigFile();

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Graphics"))
        {
            if (gbCore.cartridge.ROMLoaded && !gbCore.emulationPaused)
                ImGui::SeparatorText(FPS_text.c_str());;

#ifndef EMSCRIPTEN
            if (lockVSyncSetting) ImGui::BeginDisabled();

            if (ImGui::Checkbox("VSync", &appConfig::vsync))
            {
                glfwSwapInterval(appConfig::vsync ? 1 : 0);
                appConfig::updateConfigFile();
            }

            if (lockVSyncSetting)
            {
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Forced in GPU driver settings");

                ImGui::EndDisabled();
            }               
#endif
            if (ImGui::Checkbox("Screen Ghosting (Blending)", &appConfig::blending))
                appConfig::updateConfigFile();

            ImGui::SeparatorText("Filter");
            constexpr const char* filters[] = { "None", "LCD", "Upscaling" };

            if (ImGui::ListBox("##2", &appConfig::filter, filters, 3))
            {
                updateSelectedFilter();
                appConfig::updateConfigFile();
            }

            ImGui::SeparatorText("DMG Palette");
            constexpr const char* palettes[] = { "BGB Green", "Grayscale", "Classic" };

            if (ImGui::ListBox("##3", &appConfig::palette, palettes, 3))
            {
                updateSelectedPalette();
                appConfig::updateConfigFile();
            }

            ImGui::EndMenu();
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
                    gbCore.apu.startRecording("Recording.wav");
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
            if (ImGui::MenuItem(gbCore.emulationPaused ? "Resume" : "Pause", "(Tab)"))
                gbCore.emulationPaused = !gbCore.emulationPaused;

            if (gbCore.cartridge.ROMLoaded)
            {
                if (gbCore.cartridge.hasBattery)
                {
                    if (ImGui::MenuItem("Load Battery State"))
                        gbCore.loadBattery();
                }

                if (ImGui::MenuItem("Reset State", "Warning!"))
                {
                    gbCore.saveCurrentROM();
                    gbCore.restartROM();
                }
            }

            ImGui::EndMenu();
        }

        debugUI::updateMenu();

        if (gbCore.emulationPaused)
        {
            ImGui::Separator();
            ImGui::Text("Emulation Paused");
        }
        else if (gbCore.cartridge.ROMLoaded && gbCore.getSaveNum() != 0)
        {
            const std::string saveText = "Save: " + std::to_string(gbCore.getSaveNum());
            ImGui::Separator();
            ImGui::Text("%s", saveText.c_str());
        }

        ImGui::EndMainMenuBar();
    }

    if (showPopUp)
    {
        ImGui::OpenPopup(popupTitle);
        showPopUp = false;
        ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize(popupTitle).x + (ImGui::GetStyle().WindowPadding.x * 2), -1.0f), ImGuiCond_Appearing);
    }

    if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        ImVec2 viewportCenter = ImGui::GetMainViewport()->GetCenter();
        ImVec2 windowSize = ImGui::GetWindowSize();

        ImVec2 windowPos = ImVec2(viewportCenter.x - windowSize.x * 0.5f, viewportCenter.y - windowSize.y * 0.5f);
        ImGui::SetWindowPos(windowPos);

        const float buttonWidth = ImGui::GetContentRegionAvail().x * 0.4f;
        const float windowWidth = ImGui::GetWindowSize().x;
        ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

        if (ImGui::Button("OK", ImVec2(buttonWidth, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    debugUI::updateWindows(scaleFactor);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    updateImGUIViewports();
}

void renderGameBoy()
{
    OpenGL::bindTexture(gbFramebufferTextures[0]);
    currentShader->setFloat("alpha", 1.0f);

    if (appConfig::blending)
    {
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        currentShader->setFloat("alpha", 0.5f);
        OpenGL::bindTexture(gbFramebufferTextures[1]);
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);
    renderGameBoy();
    renderImGUI();
}

void key_callback(GLFWwindow* _window, int key, int scancode, int action, int mods)
{
    (void)_window; (void)scancode;

    if (action == 1)
    {
        if (key == GLFW_KEY_TAB)
        {
            setEmulationPaused(!gbCore.emulationPaused);
            return;
        }

        // number keys 1 though 9
        if (key >= 49 && key <= 57)
        {
            if (!gbCore.cartridge.ROMLoaded) return;

            if (mods & GLFW_MOD_ALT)
                gbCore.saveState(key - 48);

            else if (mods & GLFW_MOD_SHIFT)
                gbCore.loadState(key - 48);

            return;
        }

        if (key == GLFW_KEY_Q)
        {
            if (gbCore.cartridge.ROMLoaded)
                gbCore.saveState(gbCore.getSaveFolderPath() / "quicksave.mbs");

            return;
        }
        if (key == GLFW_KEY_GRAVE_ACCENT)
        {
            if (gbCore.cartridge.ROMLoaded)
                loadFile(gbCore.getSaveFolderPath() / "quicksave.mbs");

            return;
        }
    }
    
    gbCore.input.update(key, action);
}

void drop_callback(GLFWwindow* _window, int count, const char** paths)
{
    (void)_window;

    if (count > 0)
        loadFile(FileUtils::nativePath(paths[0]));
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
    {
        setEmulationPaused(pausedPreEvent);
        lastFrameTime = glfwGetTime(); // resetting delta time
    }
}

void window_iconify_callback(GLFWwindow* _window, int iconified)
{
    (void)_window;
    handleVisibilityChange(iconified);
}
void window_focus_callback(GLFWwindow* _window, int focused)
{
    (void)_window;
    if (!appConfig::pauseOnFocus) return;
    handleVisibilityChange(!focused);
}

void setWindowSizesToAspectRatio(float& newWidth, float& newHeight)
{
    constexpr float ASPECT_RATIO = static_cast<float>(PPU::SCR_WIDTH) / PPU::SCR_HEIGHT;
    newHeight *= 0.95f;

    if (newWidth / newHeight > ASPECT_RATIO)
        newWidth = newHeight * ASPECT_RATIO;
    else
        newHeight = newWidth / ASPECT_RATIO;

    glfwSetWindowSize(window, static_cast<int>(newWidth), static_cast<int>(newHeight + static_cast<float>(menuBarHeight)));
    glViewport(0, 0, static_cast<int>(newWidth), static_cast<int>(newHeight));
}

#ifdef EMSCRIPTEN
EM_BOOL emscripten_resize_callback(int eventType, const EmscriptenUiEvent *uiEvent, void *userData) {
    float newWidth = uiEvent->windowInnerWidth;
    float newHeight = uiEvent->windowInnerHeight;

    setWindowSizesToAspectRatio(newWidth, newHeight);
    render();
    glfwSwapBuffers(window);

    return EM_TRUE;
}
const char* unloadCallback(int eventType, const void *reserved, void *userData)
{
    if (gbCore.cartridge.ROMLoaded)
        return "Are you sure? Don't forget to export saves.";

    return "";
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
    viewport_width = width; viewport_height = height - menuBarHeight;
    glViewport(0, 0, viewport_width, viewport_height);
}
void window_pos_callback(GLFWwindow* _window, int xpos, int ypos)
{
	(void)_window; (void)xpos; (void)ypos;
    glViewport(0, 0, viewport_width, viewport_height);
}
#endif

void window_refresh_callback(GLFWwindow* _window)
{
    if (!fileDialogOpen)
    {
        render();
        glfwSwapBuffers(_window);
    }
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
			glfwSwapInterval(0);
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
    glfwSetWindowFocusCallback(window, window_focus_callback);
    glfwSetWindowRefreshCallback(window, window_refresh_callback);
    glfwSetDropCallback(window, drop_callback);
    glfwSetKeyCallback(window, key_callback);

#ifdef  EMSCRIPTEN
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, emscripten_resize_callback);
    emscripten_set_beforeunload_callback(nullptr, unloadCallback);
    emscripten_set_visibilitychange_callback(nullptr, false, visibilityChangeCallback);
#else
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowPosCallback(window, window_pos_callback);
    glfwSetWindowIconifyCallback(window, window_iconify_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
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
    float windowWidth = EM_ASM_DOUBLE ({
        return window.innerWidth;
    });
    float windowHeight = EM_ASM_DOUBLE ({
        return window.innerHeight;
    });

    setWindowSizesToAspectRatio(windowWidth, windowHeight);
#else
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    viewport_width =  { static_cast<int>(static_cast<float>(mode->width) * 0.4f) };
    viewport_height = { static_cast<int>(static_cast<float>(viewport_width) / (static_cast<float>(PPU::SCR_WIDTH) / PPU::SCR_HEIGHT)) };

    glfwSetWindowSize(window, viewport_width, viewport_height + menuBarHeight);
    glfwSetWindowAspectRatio(window, viewport_width, viewport_height);

    const uint16_t maxHeight { static_cast<uint16_t>(mode->height - mode->height / 15.0) };
    glfwSetWindowSizeLimits(window, PPU::SCR_WIDTH * 2, PPU::SCR_HEIGHT * 2, maxHeight * (PPU::SCR_WIDTH / PPU::SCR_HEIGHT), maxHeight);
    glViewport(0, 0, viewport_width, viewport_height);
#endif
}

int getScreenWidth()
{
#ifdef EMSCRIPTEN
    int screenWidth = EM_ASM_INT({
        return window.screen.width * (window.devicePixelRatio > 1.25 ? 1.25 : window.devicePixelRatio);
        });
#else
    int screenWidth = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
#endif
    return screenWidth;
}

void setImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    const int resolutionX = getScreenWidth();
    scaleFactor = (static_cast<float>(resolutionX) / 1920.0f);

    io.Fonts->AddFontFromMemoryTTF((void*)(resources::robotoMonoFont), sizeof(resources::robotoMonoFont), scaleFactor * 17.0f);
    ImGui::GetStyle().ScaleAllSizes(scaleFactor);

#ifdef __linux__
    if (std::getenv("WAYLAND_DISPLAY") == nullptr) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#else
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(GL_VERSION_STR);
}

void mainLoop()
{
    constexpr double MAX_DELTA_TIME = 0.1;

    double currentFrameTime = glfwGetTime();
    double deltaTime = currentFrameTime - lastFrameTime;

    fpsTimer += deltaTime;
    timer += std::clamp(deltaTime, 0.0, MAX_DELTA_TIME);

    if (appConfig::vsync || timer >= GBCore::FRAME_RATE)
    {
        const auto execStart = glfwGetTime();

        glfwPollEvents();
        gbCore.update(appConfig::vsync ? GBCore::calculateCycles(timer) : GBCore::CYCLES_PER_FRAME);
        render();

        frameTimes += (glfwGetTime() - execStart);
        timer = appConfig::vsync ? 0 : timer - GBCore::FRAME_RATE;
        frameCount++;

        glfwSwapBuffers(window);
    }

#ifndef EMSCRIPTEN
    if (!appConfig::vsync)
    {
        double remainder = GBCore::FRAME_RATE - timer;

        if (remainder >= 0.002f)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#endif

    if (fpsTimer >= 1.0)
    {
        double fps = frameCount / fpsTimer;
        double avgFrameTimeMs = (frameTimes / frameCount) * 1000;

        std::ostringstream oss;
        oss << "FPS: " << std::fixed << std::setprecision(2) << fps << " - " << avgFrameTimeMs << " ms";
        FPS_text = oss.str();

#ifndef  EMSCRIPTEN
        updateWindowTitle();
#endif 

        frameCount = 0;
        frameTimes = 0;
        fpsTimer = 0;

        if (!gbCore.emulationPaused && appConfig::autosaveState)
            gbCore.autoSave(); // Autosave once a second.
    }

    lastFrameTime = currentFrameTime;

    if (gbCore.emulationPaused || !gbCore.cartridge.ROMLoaded)
        glfwWaitEvents();
}

int main(int argc, char* argv[])
{
#ifndef EMSCRIPTEN
    appConfig::loadConfigFile();
#endif

    if (!setGLFW()) return -1;
    setOpenGL();
    setImGUI();
    setWindowSize();
#ifndef EMSCRIPTEN
	checkVSyncStatus();
    NFD_Init();
#endif

#ifdef EMSCRIPTEN
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
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
            int saveNum = appConfig::saveStateNum;

            if (loadFile(FileUtils::nativePath(appConfig::romPath)))
            {
                if (saveNum >= 1 && saveNum <= 9)
                    gbCore.loadState(saveNum);
            }
        }
    }

    while (!glfwWindowShouldClose(window)) { mainLoop(); }

    gbCore.saveCurrentROM();
    appConfig::updateConfigFile();

    NFD_Quit();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    glfwTerminate();
#endif

    return 0;
}