#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <nfd/nfd.hpp>

#include <iostream>
#include <filesystem>
#include <thread>

#include "GBCore.h"
#include "GBMultiplayer.h"
#include "Shader.h"
#include "shaders.h"
#include "debugUI.h"
#include "glFunctions.h"
#include "stringUtils.h"
#include "appConfig.h"
#include "resources.h"

GLFWwindow* window;

int vsyncCPUCycles;

int menuBarHeight;
int viewport_width, viewport_height;
float scaleFactor;

Shader regularShader;
Shader scalingShader;
Shader lcdShader;
std::array<uint32_t, 2> gbFramebufferTextures;

Shader* currentShader;

bool fileDialogOpen;

#ifdef _WIN32
#define STR(s) L##s
#else
#define STR(s) s
#endif

constexpr nfdnfilteritem_t openFilterItem[] = { {STR("Game ROM/Save"), STR("gb,gbc,sav,mbs")} };
constexpr nfdnfilteritem_t saveStateFilterItem[] = { {STR("Save State"), STR("mbs")} };
constexpr nfdnfilteritem_t batterySaveFilterItem[] = { {STR("Battery Save"), STR("sav")} };

#undef STR

const char* errorPopupTitle = "Error Loading the ROM!";
bool errorLoadingROM{false};

bool pauseOnVBlank {false};
std::string FPS_text{ "FPS: 00.00" };

extern GBCore gbCore;
GBMultiplayer multiplayer { gbCore };

inline bool loadFile(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path))
    {
        auto result = gbCore.loadFile(path);

        switch (result)
        {
        case FileLoadResult::InvalidROM:
            errorPopupTitle = "Error Loading the ROM!";
            errorLoadingROM = true;
            break;
        case FileLoadResult::SaveStateROMNotFound:
            errorPopupTitle = "ROM not found! Load the ROM first.";
            errorLoadingROM = true;
            break;
        case FileLoadResult::Success:
        {
            const std::string title = gbCore.gameTitle == "" ? "MegaBoy" : "MegaBoy - " + gbCore.gameTitle;
            glfwSetWindowTitle(window, title.c_str());
            debugUI::clearBuffers();
            return true;
        }
        }
    }

    return false;
}

void drawCallback(const uint8_t* framebuffer)
{
    if (pauseOnVBlank)
    {
        pauseOnVBlank = false;
        gbCore.emulationPaused = true;
    }

    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuffer);
    std::swap(gbFramebufferTextures[0], gbFramebufferTextures[1]);

    debugUI::updateTextures(gbCore.emulationPaused);
}

void refreshGBTextures()
{
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getFrameBuffer());
    OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getFrameBuffer());
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
            lcdShader.compile(shaders::lcd1xVertexShader, shaders::lcd1xFragmentShader);
            lcdShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
            break;
        case 2:
            scalingShader.compile(shaders::omniscaleVertexShader, shaders::omniscaleFragmentShader);
            scalingShader.setFloat2("OutputSize", PPU::SCR_WIDTH * 6, PPU::SCR_HEIGHT * 6);
            scalingShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
            break;
        default:
            regularShader.compile(shaders::regularVertexShader, shaders::regulaFragmentShader);
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

void updateSelectedPalette()
{
    auto colors = appConfig::palette == 0 ? PPU::BGB_GREEN_PALETTE : appConfig::palette == 1 ? PPU::GRAY_PALETTE : PPU::CLASSIC_PALETTE;
    if (gbCore.emulationPaused || !gbCore.cartridge.ROMLoaded) gbCore.ppu.updateScreenColors(colors);

    gbCore.ppu.setColorsPalette(colors);
    refreshGBTextures();
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
                fileDialogOpen = true;
                NFD::UniquePathN outPath;

                nfdresult_t result = NFD::OpenDialog(outPath, openFilterItem, 1);

                if (result == NFD_OKAY)
                    loadFile(outPath.get());

                fileDialogOpen = false;
            }

            if (gbCore.cartridge.ROMLoaded)
            {
                if (ImGui::MenuItem("Export State"))
                {
                    fileDialogOpen = true;
                    NFD::UniquePathN outPath;

                    const auto defaultName = StringUtils::nativePath(gbCore.gameTitle + " - Save State");
                    nfdresult_t result = NFD::SaveDialog(outPath, saveStateFilterItem, 1, nullptr, defaultName.c_str());

                    if (result == NFD_OKAY)
                        gbCore.saveState(outPath.get());

                    fileDialogOpen = false;
                }

                if (gbCore.cartridge.hasBattery)
                {
                    if (ImGui::MenuItem("Export Battery"))
                    {
                        fileDialogOpen = true;
                        NFD::UniquePathN outPath;

                        const auto defaultName = StringUtils::nativePath(gbCore.gameTitle + " - Battery Save");
                        nfdresult_t result = NFD::SaveDialog(outPath, batterySaveFilterItem, 1, nullptr, defaultName.c_str());

                        if (result == NFD_OKAY)
                            gbCore.saveBattery(outPath.get());

                        fileDialogOpen = false;
                    }
                }
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings", "Ctrl+Q"))
        {
            if (ImGui::Checkbox("Load ROM on Startup", &appConfig::loadLastROM))
                appConfig::updateConfigFile();

            if (ImGui::Checkbox("Run Boot ROM", &appConfig::runBootROM))
                appConfig::updateConfigFile();

            if (ImGui::Checkbox("Pause when unfocused", &appConfig::pauseOnFocus))
                appConfig::updateConfigFile();

            ImGui::SeparatorText("Saves");

            if (ImGui::Checkbox("Battery Saves", &appConfig::batterySaves))
                appConfig::updateConfigFile();

            if (ImGui::Checkbox("Autosave State", &appConfig::autosaveState))
                appConfig::updateConfigFile();

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Graphics"))
        {
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

            ImGui::SeparatorText("UI");

            if (ImGui::Checkbox("Screen Ghosting", &appConfig::blending))
            {
                updateSelectedBlending();
                appConfig::updateConfigFile();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            constexpr const char* filters[] = { "None", "LCD", "Upscaling" };

            if (ImGui::ListBox("Filter", &appConfig::filter, filters, 3))
            {
                updateSelectedFilter();
                appConfig::updateConfigFile();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            constexpr const char* palettes[] = { "BGB Green", "Grayscale", "Classic" };

            if (ImGui::ListBox("Palette", &appConfig::palette, palettes, 3))
            {
                updateSelectedPalette();
                appConfig::updateConfigFile();
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
                    if (ImGui::MenuItem("Load Battery"))
                        gbCore.loadBattery();
                }

                if (ImGui::MenuItem("Reset State"))
                {
                    gbCore.saveCurrentROM();
                    gbCore.restartROM();
                }
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Multiplayer"))
        {
            if (ImGui::MenuItem("Host"))
                multiplayer.host();

            if (ImGui::MenuItem("Join"))
                multiplayer.connect("127.0.0.1");

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

        if (gbCore.cartridge.ROMLoaded && !gbCore.emulationPaused)
        {
            float text_width = ImGui::CalcTextSize(FPS_text.data()).x;
            float available_width = ImGui::GetContentRegionAvail().x;

            if (text_width < available_width)
            {
                ImGui::SameLine(ImGui::GetWindowWidth() - text_width - ImGui::GetStyle().ItemSpacing.x * 3);
                ImGui::Separator();
                ImGui::Text(FPS_text.data());
            }
        }

        ImGui::EndMainMenuBar();
    }

    if (errorLoadingROM)
    {
        ImGui::OpenPopup(errorPopupTitle);
        errorLoadingROM = false;
    }

    if (ImGui::BeginPopupModal(errorPopupTitle, 0, ImGuiWindowFlags_NoMove))
    {
        float windowWidth = ImGui::GetWindowSize().x;
        ImGui::SetCursorPosX((windowWidth - (75.0f * scaleFactor)) * 0.5f);

        if (ImGui::Button("Ok", ImVec2(75 * scaleFactor, 30 * scaleFactor)))
            ImGui::CloseCurrentPopup();

        ImGui::SetWindowSize(ImVec2(ImGui::CalcTextSize(errorPopupTitle).x + ImGui::GetStyle().FramePadding.x, ImGui::GetContentRegionAvail().y));
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

void framebuffer_size_callback(GLFWwindow* _window, int width, int height)
{
    (void)_window;
    viewport_width = width; viewport_height = height - menuBarHeight;
    glViewport(0, 0, viewport_width, viewport_height);
}

void window_refresh_callback(GLFWwindow* _window)
{
    (void)_window;
    if (!fileDialogOpen) render(); // Super strange issue - ImGUI crashes if rendering is done while file dialog is open???
}

bool pausedPreEvent;

void window_iconify_callback(GLFWwindow* _window, int iconified)
{
    (void)_window;

    if (iconified)
    {
        pausedPreEvent = gbCore.emulationPaused;
        gbCore.emulationPaused = true;
    }
    else
        gbCore.emulationPaused = pausedPreEvent;
}
void window_focus_callback(GLFWwindow* _window, int focused)
{
    (void)_window;

    if (!appConfig::pauseOnFocus) return;

    if (!focused)
    {
        pausedPreEvent = gbCore.emulationPaused;
        gbCore.emulationPaused = true;
    }
    else
        gbCore.emulationPaused = pausedPreEvent;
}

void key_callback(GLFWwindow* _window, int key, int scancode, int action, int mods)
{
    (void)_window;

    if (action == 1)
    {
        if (key == GLFW_KEY_TAB)
        {
            if (gbCore.emulationPaused) gbCore.emulationPaused = false;
            else
            {
                if (gbCore.cartridge.ROMLoaded) pauseOnVBlank = true;
                else gbCore.emulationPaused = true;
            }
            return;
        }
        // number keys 1 though 0
        if (scancode >= 2 && scancode <= 11)
        {
            if (!gbCore.cartridge.ROMLoaded) return;

            if (mods & GLFW_MOD_CONTROL)
                gbCore.saveState(scancode - 1);

            else if (mods & GLFW_MOD_SHIFT)
                gbCore.loadState(scancode - 1);

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

            //multiplayer.testMessage();
            return;
        }
    }

    if (!gbCore.emulationPaused)
        gbCore.input.update(scancode, action);
}

void drop_callback(GLFWwindow* _window, int count, const char** paths)
{
    (void)_window;

    if (count > 0)
        loadFile(StringUtils::nativePath(std::string(paths[0])));
}

bool setGLFW()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1, 1, "MegaBoy", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(appConfig::vsync ? 1 : 0);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowIconifyCallback(window, window_iconify_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);
    glfwSetWindowRefreshCallback(window, window_refresh_callback);
    glfwSetDropCallback(window, drop_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

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

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    vsyncCPUCycles = GBCore::calculateCycles(1.0 / mode->refreshRate);

    viewport_width = { static_cast<int>(mode->width * 0.4f) };
    viewport_height = { static_cast<int>(viewport_width / (static_cast<float>(PPU::SCR_WIDTH) / PPU::SCR_HEIGHT)) };

    glfwSetWindowSize(window, viewport_width, viewport_height + menuBarHeight);
    glfwSetWindowAspectRatio(window, viewport_width, viewport_height);

    uint16_t maxHeight{ static_cast<uint16_t>(mode->height - mode->height / 15.0) };
    glfwSetWindowSizeLimits(window, PPU::SCR_WIDTH * 2, PPU::SCR_HEIGHT * 2, maxHeight * (PPU::SCR_WIDTH / PPU::SCR_HEIGHT), maxHeight);
    glViewport(0, 0, viewport_width, viewport_height);

    const auto clearColor = gbCore.ppu.getCurrentPalette()[0];
    glClearColor(clearColor.R, clearColor.G, clearColor.B, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

const std::string imguiConfigPath = StringUtils::pathToUTF8(StringUtils::executableFolderPath / "data" / "imgui.ini");

void setImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = imguiConfigPath.c_str();

    const int resolutionX = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
    scaleFactor = (resolutionX / 1920.0f);

    io.Fonts->AddFontFromMemoryTTF((void*)resources::robotoMonoFont, sizeof(resources::robotoMonoFont), scaleFactor * 17.0f);
    ImGui::GetStyle().ScaleAllSizes(scaleFactor);

    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

//void compareFiles()
//{
//    std::ifstream newV("megaboyLog.txt");
//    std::ifstream oldV("otherLogs.txt");
//
//    std::vector<std::string> newLines;
//    std::vector<std::string> oldLines;
//
//    newLines.reserve(400000);
//    oldLines.reserve(400000);
//
//    std::string line;
//
//    while (std::getline(newV, line))
//    {
//        newLines.push_back(line);
//    }
//
//    while (std::getline(oldV, line))
//    {
//        oldLines.push_back(line);
//    }
//
//    for (int i = 0; i < 400000; i++)
//    {
//        if (newLines[i] != oldLines[i])
//        {
//            std::cout << "Difference at line " << i + 1 << "\n";
//            std::cout << newLines[i] << "\n";
//            return;
//        }
//    }
//
//    std::cout << "No differences! \n";
//}

int main(int argc, char* argv[])
{
    appConfig::loadConfigFile();

    if (!setGLFW()) return -1;
    setImGUI();
    setWindowSize();
    setBuffers();

    if (argc > 1)
    {
#ifdef _WIN32
        auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
#endif
        loadFile(argv[1]);
    }
    else
    {
        if (appConfig::loadLastROM && appConfig::romPath != "")
        {
            int saveNum = appConfig::saveStateNum;

            if (loadFile(StringUtils::nativePath(appConfig::romPath)))
            {
                if (saveNum >= 0 && saveNum <= 10)
                    gbCore.loadState(saveNum);
            }
        }
    }

    double lastFrameTime = glfwGetTime();
    double fpsTimer{};
    double timer{};
    int frameCount{};

    while (!glfwWindowShouldClose(window))
    {
        double currentFrameTime = glfwGetTime();
        double deltaTime = currentFrameTime - lastFrameTime;

        fpsTimer += deltaTime;
        timer += deltaTime;

        const bool updateCPU = appConfig::vsync || timer >= GBCore::FRAME_RATE;  
        const bool updateRender = updateCPU || (!appConfig::vsync && !appConfig::fpsLock);

        if (updateCPU)
        {
            gbCore.update(appConfig::vsync ? vsyncCPUCycles : GBCore::CYCLES_PER_FRAME);
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
            FPS_text = "FPS: " + std::format("{:.2f}", fps);

            frameCount = 0;
            fpsTimer = 0;

            if (!gbCore.emulationPaused && appConfig::autosaveState) 
                gbCore.autoSave(); // Autosave once a second.
        }

        lastFrameTime = currentFrameTime;
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

        if (gbCore.emulationPaused || !gbCore.cartridge.ROMLoaded)
            glfwWaitEvents();
    }

    gbCore.saveCurrentROM();
    appConfig::updateConfigFile();
    return 1;
}