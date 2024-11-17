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

#include "GBCore.h"
#include "gbSystem.h"
#include "appConfig.h"
#include "keyBindManager.h"
#include "debugUI.h"
#include "resources.h"
#include "Utils/Shader.h"
#include "Utils/fileUtils.h"
#include "Utils/glFunctions.h"

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
std::array<uint32_t, 2> gbFramebufferTextures;

Shader* currentShader{ };

bool fileDialogOpen { false };

#ifndef EMSCRIPTEN
#ifdef _WIN32
#define STR(s) L##s
#else
#define STR(s) s
#endif

constexpr nfdnfilteritem_t openFilterItem[] = { {STR("Game ROM/Save"), STR("gb,gbc,sav,mbs")} };
constexpr nfdnfilteritem_t saveStateFilterItem[] = { {STR("Save State"), STR("mbs")} };
constexpr nfdnfilteritem_t batterySaveFilterItem[] = { {STR("Battery Save"), STR("sav")} };
constexpr nfdnfilteritem_t audioSaveFilterItem[] = { {STR("WAV File"), STR("wav")} };

#undef STR
#else
constexpr const char* openFilterItem = ".gb,.gbc,.sav,.mbs";
#endif

const char* popupTitle = "";
bool showPopUp { false };

std::string FPS_text{ "FPS: 00.00 - 0.00 ms" };

extern GBCore gbCore;

constexpr float FADE_DURATION = 0.8f;
bool fadeEffectActive { false };
float fadeTime { 0.0f };

bool fastForwarding { false };
constexpr int FAST_FORWARD_SPEED = 5;

double lastFrameTime = glfwGetTime();
double secondsTimer{};
double gbTimer{};
uint32_t frameCount{};
uint32_t cycleCount{};
double executeTimes{};

bool lockVSyncSetting { false };

int awaitingKeyBind { -1 };

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

void handleBootROMLoad(std::string& destRomPath, const std::filesystem::path& filePath)
{
    if (GBCore::isBootROMValid(filePath))
    {
        destRomPath = filePath.string();
        appConfig::updateConfigFile();
        popupTitle = "Successfully Loaded Boot ROM!";
    }
    else
        popupTitle = "Invalid Boot ROM!";

    showPopUp = true;
}

std::filesystem::path currentFilePath{};
inline bool loadFile(const std::filesystem::path& path)
{
    if (std::ifstream ifs { path })
    {
        ifs.close();

        if (path.filename() == GBCore::DMG_BOOTROM_NAME)
        {
            handleBootROMLoad(appConfig::dmgBootRomPath, path);
            return true;
        }
        if (path.filename() == GBCore::CGB_BOOTROM_NAME)
        {
            handleBootROMLoad(appConfig::cgbBootRomPath, path);
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
            if (currentFilePath.filename() != GBCore::DMG_BOOTROM_NAME && currentFilePath.filename() != GBCore::CGB_BOOTROM_NAME)
            {
                debugUI::signalROMLoaded();
#ifdef EMSCRIPTEN
                if (path != currentFilePath)
					std::filesystem::remove(currentFilePath);
#endif
            }
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

void drawCallback(const uint8_t* framebuffer)
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuffer);
    std::swap(gbFramebufferTextures[0], gbFramebufferTextures[1]);
    debugUI::signalVBlank();
}

void refreshGBTextures()
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->getFrameBuffer());
    OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->getFrameBuffer());
}

void updateSelectedPalette()
{
    const auto& newColors = appConfig::palette == 0 ? PPU::BGB_GREEN_PALETTE : appConfig::palette == 1 ? PPU::GRAY_PALETTE :
                            appConfig::palette == 2 ? PPU::CLASSIC_PALETTE : PPU::CUSTOM_PALETTE;

    if (gbCore.emulationPaused && System::Current() == GBSystem::DMG)
    {
        gbCore.ppu->refreshDMGScreenColors(newColors);
        refreshGBTextures();
    }

    PPU::ColorPalette = newColors.data();
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
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const std::vector<uint8_t> whiteBG(PPU::FRAMEBUFFER_SIZE, 255);

    OpenGL::createTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, whiteBG.data(), appConfig::bilinearFiltering);
    OpenGL::createTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, whiteBG.data(), appConfig::bilinearFiltering);

    updateSelectedFilter();
    updateSelectedPalette(); 

    gbCore.setDrawCallback(drawCallback);
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
    if (newScale == 0)
        return;

    int newWindowWidth = newScale * PPU::SCR_WIDTH;
    int newWindowHeight = newScale * PPU::SCR_HEIGHT + menuBarHeight;

#ifndef EMSCRIPTEN
    if (newWindowWidth > getResolutionX() || newWindowHeight > getResolutionY())
        return;

    if (!glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) // if window is maximized, then just change the viewport size, don't change the window size.
    {
        glfwSetWindowSize(window, newWindowWidth, newWindowHeight);
        currentIntegerScale = newScale;
        return;
    }
#else
    if (newWindowWidth > window_width || newWindowHeight > window_height)
        return;
#endif

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

inline bool isBootROMFile(const std::string& fileName) { return fileName == GBCore::DMG_BOOTROM_NAME || fileName == GBCore::CGB_BOOTROM_NAME; }

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
    inputFile.read(buffer.data(), static_cast<std::streamsize>(fileSize));

    emscripten_browser_file::download(fileName, "application/octet-stream", buffer.data(), fileSize);
    std::filesystem::remove(fileName);
}

void handle_upload_file(std::string const &filename, std::string const &mime_type, std::string_view buffer, void*)
{
    std::ofstream fs(filename, std::ios::out | std::ios::binary);
    fs.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
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

    static bool keyConfigWindowOpen { false };

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
            static bool bootRomsExist{ false };

            if (ImGui::IsWindowAppearing())
                bootRomsExist = std::filesystem::exists(FileUtils::nativePath(appConfig::dmgBootRomPath)) || std::filesystem::exists(FileUtils::nativePath(appConfig::cgbBootRomPath));

            if (!bootRomsExist)
            {
                ImGui::BeginDisabled();
                ImGui::Checkbox("Boot ROMs not Loaded!", &bootRomsExist);

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Drop 'dmg_boot.bin' or 'cgb_boot.bin'");

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

            if (ImGui::Checkbox("Autosave Save State", &appConfig::autosaveState))
                appConfig::updateConfigFile();

#ifndef EMSCRIPTEN
            if (ImGui::Checkbox("Create Backups", &appConfig::backupSaves))
                appConfig::updateConfigFile();
#endif

            ImGui::SeparatorText("Controls");

            if (ImGui::Button("Key Binding"))
                keyConfigWindowOpen = true;

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

            static bool showPaletteSelection { false };

            if (ImGui::ArrowButton("##3", ImGuiDir_Right))
                showPaletteSelection = !showPaletteSelection;

            ImGui::SameLine();
            ImGui::SeparatorText("DMG Palette");

            if (showPaletteSelection)
            {
                constexpr std::array<const char*, 4> palettes = { "BGB Green", "Grayscale", "Classic", "Custom" };

                static bool customPaletteOpen{ false };
                static std::array<std::array<float, 3>, 4> colors{ };

                ImGui::Spacing();

                if (ImGui::ListBox("##4", &appConfig::palette, palettes.data(), palettes.size()))
                {
                    if (appConfig::palette == 3)
                    {
                        for (int i = 0; i < 4; i++)
                            colors[i] = { PPU::CUSTOM_PALETTE[i].R / 255.0f, PPU::CUSTOM_PALETTE[i].G / 255.0f, PPU::CUSTOM_PALETTE[i].B / 255.0f };

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
                            PPU::CUSTOM_PALETTE[i] =
                            {
                                static_cast<uint8_t>(colors[i][0] * 255),
                                static_cast<uint8_t>(colors[i][1] * 255),
                                static_cast<uint8_t>(colors[i][2] * 255)
                            };

                            appConfig::updateConfigFile();
                        }
                    }

                    ImGui::End();
                }
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
                    if (ImGui::MenuItem("Reset to Battery"))
                        gbCore.resetToBattery();
                }

                if (ImGui::MenuItem("Reset State", "Warning!"))
                    gbCore.restartROM();
            }

            ImGui::EndMenu();
        }

        debugUI::updateMenu();

        if (gbCore.emulationPaused)
        {
            ImGui::Separator();
            ImGui::Text("Emulation Paused");
        }
        else if (fastForwarding)
        {
			ImGui::Separator();
			ImGui::Text("Fast Forward...");
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

    if (keyConfigWindowOpen)
    {
        if (ImGui::Begin("Key Configuration", &keyConfigWindowOpen, ImGuiWindowFlags_NoResize))
        {
            constexpr int totalItems = KeyBindManager::TOTAL_KEYS;
            constexpr int itemsPerColumn = (totalItems + 1) / 2;
            const float padding = ImGui::GetStyle().FramePadding.x * 2;

            auto calculateColumnWidths = [&]()
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

            auto [maxWidthCol1, maxWidthCol2] = calculateColumnWidths();

            if (ImGui::BeginTable("KeyBindTable", 2, ImGuiTableFlags_NoKeepColumnsVisible | ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, maxWidthCol1 + 130 * scaleFactor + padding);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, maxWidthCol2 + 130 * scaleFactor);

                auto renderKeyBinding = [&](int index, float maxWidth) 
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
            loadFile(currentFilePath);
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

    if (!gbCore.cartridge.ROMLoaded) return;

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
            if (mods & GLFW_MOD_ALT)
                gbCore.saveState(key - 48);

            else if (mods & GLFW_MOD_SHIFT)
                gbCore.loadState(key - 48);
        }

        if (key == KeyBindManager::getBind(MegaBoyKey::QuickSave))
        {
            gbCore.saveState(gbCore.getSaveStateFolderPath() / "quicksave.mbs");
            return;
        }
        if (key == KeyBindManager::getBind(MegaBoyKey::LoadQuickSave))
        {
            loadFile(gbCore.getSaveStateFolderPath() / "quicksave.mbs");
            return;
        }
    }

    if (key == KeyBindManager::getBind(MegaBoyKey::Restart))
    {
        if (action == GLFW_PRESS)
            fadeEffectActive = true;
        else if (action == GLFW_RELEASE)
            resetFade();
    }

    if (key == KeyBindManager::getBind(MegaBoyKey::FastForward))
    {
        if (action == GLFW_PRESS)
			fastForwarding = true;
		else if (action == GLFW_RELEASE)
			fastForwarding = false;
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
    std::cout << "in resize callback!\n";
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

const char* unloadCallback(int eventType, const void *reserved, void *userData)
{
    gbCore.autoSave();
    return gbCore.getSaveNum() != 0 ? "Are you sure? Don't forget to export saves." : "";
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
    glfwSetWindowRefreshCallback(window, window_refresh_callback);
    glfwSetDropCallback(window, drop_callback);
    glfwSetKeyCallback(window, key_callback);

#ifdef  EMSCRIPTEN
    glfwSetWindowTitle(window, "MegaBoy");

    glfwSetWindowContentScaleCallback(window, content_scale_callback);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, emscripten_resize_callback);
    emscripten_set_beforeunload_callback(nullptr, unloadCallback);
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

    double currentFrameTime = glfwGetTime();
    double deltaTime = currentFrameTime - lastFrameTime;

    secondsTimer += deltaTime;
    gbTimer += std::clamp(deltaTime, 0.0, MAX_DELTA_TIME);

    if (appConfig::vsync || gbTimer >= GBCore::FRAME_RATE)
    {
        glfwPollEvents();

        const uint32_t cycles = appConfig::vsync ? GBCore::calculateCycles(gbTimer) : GBCore::CYCLES_PER_FRAME;
        const auto execStart = glfwGetTime();
        gbCore.update(cycles * (fastForwarding ? FAST_FORWARD_SPEED : 1));

        executeTimes += (glfwGetTime() - execStart);
        cycleCount += cycles;
        frameCount++;
        gbTimer = appConfig::vsync ? 0 : gbTimer - GBCore::FRAME_RATE;

        render(deltaTime);
    }

#ifndef EMSCRIPTEN
    if (!appConfig::vsync)
    {
        const double remainder = GBCore::FRAME_RATE - gbTimer;

        if (remainder >= 0.002f)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#endif

    if (secondsTimer >= 1.0)
    {
        const double totalGBFrames = static_cast<double>(cycleCount) / GBCore::CYCLES_PER_FRAME;
        const double avgGBExecuteTime = (executeTimes / totalGBFrames) * 1000;
        const double fps = frameCount / secondsTimer;

        std::ostringstream oss;
        oss << "FPS: " << std::fixed << std::setprecision(2) << fps << " - " << avgGBExecuteTime << " ms";
        FPS_text = oss.str();

#ifndef  EMSCRIPTEN
        updateWindowTitle();
#endif 

        cycleCount = 0;
        frameCount = 0;
        executeTimes = 0;
        secondsTimer = 0;

         if (!gbCore.emulationPaused && appConfig::autosaveState)
             gbCore.autoSave(); // Autosave once a second.
    }

    lastFrameTime = currentFrameTime;

    if ((gbCore.emulationPaused && !fadeEffectActive) || !gbCore.cartridge.ROMLoaded)
    {
        glfwWaitEvents(); // To reduce CPU usage when paused.
        lastFrameTime = glfwGetTime();
    }
}

#ifdef EMSCRIPTEN
extern "C" EMSCRIPTEN_KEEPALIVE void loadAppConfigFile()
{
    appConfig::loadConfigFile();
}
#endif

int main(int argc, char* argv[])
{
#ifdef EMSCRIPTEN
    gbCore.setBatterySaveFolder("batterySaves");

    EM_ASM ({
        FS.mkdir('/batterySaves');
        FS.mount(IDBFS, { autoPersist: true }, '/batterySaves');
        FS.mkdir('/data');
        FS.mount(IDBFS, { autoPersist: true }, '/data');

        FS.syncfs(true, function(err) {
            if (err != null) {
                console.log(err);
            } else {
                Module.ccall('loadAppConfigFile', null, [], []);
            }
        });
    });
#else
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
            const int saveNum = appConfig::saveStateNum;

            if (loadFile(FileUtils::nativePath(appConfig::romPath)))
            {
                if (saveNum >= 1 && saveNum <= 9)
                    gbCore.loadState(saveNum);
            }
        }
    }

    while (!glfwWindowShouldClose(window)) { mainLoop(); }

    gbCore.saveAndBackup();
    appConfig::updateConfigFile();

    NFD_Quit();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    glfwTerminate();
#endif

    return 0;
}