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

#include <iostream>
#include <filesystem>
#include <optional>

#include "GBCore.h"
#include "appConfig.h"
#include "debugUI.h"
#include "resources.h"
#include "Utils/Shader.h"
#include "Utils/stringUtils.h"
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

std::string FPS_text{ "FPS: 00.00" };

extern GBCore gbCore;
//GBMultiplayer multiplayer { gbCore };

double lastFrameTime = glfwGetTime();
double fpsTimer{};
double timer{};
int frameCount{};

inline void updateWindowTitle()
{
    std::string title = (gbCore.gameTitle.empty() ? "MegaBoy" : "MegaBoy - " + gbCore.gameTitle);
    if (gbCore.cartridge.ROMLoaded) title += !gbCore.emulationPaused ? (" (" + FPS_text + ")") : "";
    glfwSetWindowTitle(window, title.c_str());
}

inline void setEmulationPaused(bool val)
{
    gbCore.emulationPaused = val;
    if (val) debugUI::updateTextures(true);
    updateWindowTitle();
}

constexpr const char* DMG_ROM_NAME = "dmg_boot.bin";
constexpr const char* CGB_ROM_NAME = "cgb_boot.bin";

void handleBootROMLoad(std::string& romPath, const std::filesystem::path filePath)
{
    romPath = filePath.string();
    appConfig::updateConfigFile();

    popupTitle = "Successfully Loaded Boot ROM!";
    showPopUp = true;
}

std::filesystem::path currentFilePath{};
inline bool loadFile(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path))
    {
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

        const auto result = gbCore.loadFile(path);

        switch (result)
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
            debugUI::clearBuffers();
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

void drawCallback(const uint8_t* framebuffer)
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuffer);
    std::swap(gbFramebufferTextures[0], gbFramebufferTextures[1]);
    debugUI::updateTextures(false);
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
            lcdShader.compile(resources::lcd1xVertexShader, resources::lcd1xFragmentShader);
            lcdShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
            break;
        case 2:
            scalingShader.compile(resources::omniscaleVertexShader, resources::omniscaleFragmentShader);
            scalingShader.setFloat2("OutputSize", PPU::SCR_WIDTH * 6, PPU::SCR_HEIGHT * 6);
            scalingShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
            break;
        default:
            regularShader.compile(resources::regularVertexShader, resources::regulaFragmentShader);
            break;
        }
    }
}

void updateSelectedBlending()
{
    if (appConfig::blending)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
        currentShader->setFloat("alpha", 1.0f);
    }
}

void refreshGBTextures()
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getFrameBuffer());
    OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getFrameBuffer());
}

void updateSelectedPalette()
{
    auto colors = appConfig::palette == 0 ? PPU::BGB_GREEN_PALETTE : appConfig::palette == 1 ? PPU::GRAY_PALETTE : PPU::CLASSIC_PALETTE;
    if (gbCore.emulationPaused)
    {
        gbCore.ppu.updateDMG_ScreenColors(colors);
        refreshGBTextures();
    }

    gbCore.ppu.setDMGPalette(colors);
}

void setBuffers()
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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    OpenGL::createTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
    OpenGL::createTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
    refreshGBTextures();

    gbCore.ppu.drawCallback = drawCallback;

    updateSelectedFilter();
    updateSelectedPalette();
    updateSelectedBlending();
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

constexpr bool isBootROMFile(const std::string& fileName) { return fileName == "dmg_boot.bin" || fileName == "cgb_boot.bin"; }

#ifndef EMSCRIPTEN
std::optional<std::filesystem::path> saveFileDialog(const std::string& defaultName, const nfdnfilteritem_t* filter)
{
    fileDialogOpen = true;
    NFD::UniquePathN outPath;

    const auto NdefaultName = StringUtils::nativePath(defaultName);
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
                romsExist = std::filesystem::exists(StringUtils::nativePath(appConfig::dmgRomPath)) || std::filesystem::exists(StringUtils::nativePath(appConfig::cgbRomPath));

            if (!romsExist)
            {
                ImGui::BeginDisabled();
                ImGui::Checkbox("Boot ROMs not Loaded!", &romsExist);
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
#ifndef EMSCRIPTEN
            if (ImGui::Checkbox("VSync", &appConfig::vsync))
            {
                glfwSwapInterval(appConfig::vsync ? 1 : 0);
                appConfig::updateConfigFile();
            }

            if (!appConfig::vsync)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Checkbox("FPS Lock", &appConfig::fpsLock))
                    appConfig::updateConfigFile();
            }
#endif
            ImGui::SeparatorText("UI");

            if (ImGui::Checkbox("Screen Ghosting", &appConfig::blending))
            {
                updateSelectedBlending();
                appConfig::updateConfigFile();
            }

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

            ImGui::Checkbox("Enable Audio", &gbCore.apu.enableAudio);
            static int volume { static_cast<int>(gbCore.apu.volume * 100) };

            if (ImGui::SliderInt("Volume", &volume, 0, 100))
                gbCore.apu.volume = volume / 100.0;

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
            std::string text = "Save: " + std::to_string(gbCore.getSaveNum());
            ImGui::Separator();
            ImGui::Text(text.c_str());
        }

        ImGui::EndMainMenuBar();
    }

    if (showPopUp)
    {
        ImGui::OpenPopup(popupTitle);
        showPopUp = false;
        ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize(popupTitle).x + (ImGui::GetStyle().WindowPadding.x * 2), -1.0f), ImGuiCond_Appearing);
    }

    if (ImGui::BeginPopupModal(popupTitle, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
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

    if (appConfig::blending)
    {
        currentShader->setFloat("alpha", 1.0f);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        currentShader->setFloat("alpha", 0.5f);
        OpenGL::bindTexture(gbFramebufferTextures[1]);
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);
    renderGameBoy();
    renderImGUI();
    glfwSwapBuffers(window);
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
        // number keys 1 though 0
        if (key >= 48 && key <= 57)
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
        loadFile(StringUtils::nativePath(std::string(paths[0])));
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

void setWindowSizesToAspectRatio(int& newWidth, int& newHeight)
{
    constexpr float ASPECT_RATIO = static_cast<float>(PPU::SCR_WIDTH) / PPU::SCR_HEIGHT;
    newHeight = static_cast<int>(newHeight * 0.95f);

    if ((newWidth / static_cast<float>(newHeight)) > ASPECT_RATIO)
        newWidth = static_cast<int>(newHeight * ASPECT_RATIO);
    else
        newHeight = static_cast<int>(newWidth / ASPECT_RATIO);

    glfwSetWindowSize(window, newWidth, newHeight + menuBarHeight);
    glViewport(0, 0, newWidth, newHeight);
}

#ifdef EMSCRIPTEN
EM_BOOL emscripten_resize_callback(int eventType, const EmscriptenUiEvent *uiEvent, void *userData) {
    int newWidth = uiEvent->windowInnerWidth;
    int newHeight = uiEvent->windowInnerHeight;

    setWindowSizesToAspectRatio(newWidth, newHeight);
    render();

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
#endif

void window_refresh_callback(GLFWwindow* _window)
{
    (void)_window;
    if (!fileDialogOpen) render(); // ImGUI crashes if rendering is done while file dialog is open
}

bool setGLFW()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);

    window = glfwCreateWindow(1, 1, "MegaBoy", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(appConfig::vsync ? 1 : 0);
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
    int windowWidth = EM_ASM_INT ({
        return window.innerWidth;
    });
    int windowHeight = EM_ASM_INT ({
        return window.innerHeight;
    });

    setWindowSizesToAspectRatio(windowWidth, windowHeight);
#else
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    viewport_width =  { static_cast<int>(mode->width * 0.4f) };
    viewport_height = { static_cast<int>(viewport_width / (static_cast<float>(PPU::SCR_WIDTH) / PPU::SCR_HEIGHT)) };

    glfwSetWindowSize(window, viewport_width, viewport_height + menuBarHeight);
    glfwSetWindowAspectRatio(window, viewport_width, viewport_height);

    uint16_t maxHeight{ static_cast<uint16_t>(mode->height - mode->height / 15.0) };
    glfwSetWindowSizeLimits(window, PPU::SCR_WIDTH * 2, PPU::SCR_HEIGHT * 2, maxHeight * (PPU::SCR_WIDTH / PPU::SCR_HEIGHT), maxHeight);
    glViewport(0, 0, viewport_width, viewport_height);
#endif
}

int getScreenWidth()
{
#ifdef EMSCRIPTEN
    int screenWidth = EM_ASM_INT ({
        return window.screen.width * (window.devicePixelRatio > 1.25 ? 1.25 : window.devicePixelRatio);
    });
#else
    int screenWidth = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
#endif
    return screenWidth;
}

const std::string imguiConfigPath = StringUtils::pathToUTF8(StringUtils::executableFolderPath / "data" / "imgui.ini");

void setImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = imguiConfigPath.c_str();

    const int resolutionX = getScreenWidth();
    scaleFactor = (resolutionX / 1920.0f);

    io.Fonts->AddFontFromMemoryTTF((void*)resources::robotoMonoFont, sizeof(resources::robotoMonoFont), scaleFactor * 17.0f);
    ImGui::GetStyle().ScaleAllSizes(scaleFactor);

    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 300 es");
}

void mainLoop()
{
    constexpr double MAX_DELTA_TIME = 0.1;

    double currentFrameTime = glfwGetTime();
    double deltaTime = currentFrameTime - lastFrameTime;

    fpsTimer += deltaTime;
    timer += std::clamp(deltaTime, 0.0, MAX_DELTA_TIME);

    const bool updateCPU = appConfig::vsync || timer >= GBCore::FRAME_RATE;
    const bool updateRender = updateCPU || (!appConfig::vsync && !appConfig::fpsLock);

    if (updateCPU)
    {
        gbCore.update(GBCore::calculateCycles(timer));
        timer = 0;
    }

    if (updateRender)
    {
        glfwPollEvents();
        render();
        frameCount++;
    }

    if (fpsTimer >= 1.0)
    {
        double fps = frameCount / fpsTimer;
        std::ostringstream oss;
        oss << "FPS: " << std::fixed << std::setprecision(2) << fps;
        FPS_text = oss.str();
        updateWindowTitle();

        frameCount = 0;
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
    setImGUI();
#ifndef EMSCRIPTEN
    NFD_Init();
#endif
    setWindowSize();
    setBuffers();

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

            if (loadFile(StringUtils::nativePath(appConfig::romPath)))
            {
                if (saveNum >= 0 && saveNum <= 10)
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