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
#include "Shader.h"
#include "debugUI.h"
#include "glFunctions.h"
#include "stringUtils.h"

GLFWwindow* window;

bool blending{ false };
bool pauseOnFocus { false };
bool autosaves { true };

bool fpsLock{ true };
bool vsync { true };
int vsyncCPUCycles;

int menuBarHeight;
int viewport_width, viewport_height;
float scaleFactor;

Shader regularShader;
Shader scalingShader;
Shader lcdShader;
std::array<uint32_t, 2> gbFramebufferTextures;

Shader* currentShader;

#ifdef _WIN32
const std::wstring defaultPath{ std::filesystem::current_path().wstring() };
#else
const std::string defaultPath{ std::filesystem::current_path().string() };
#endif 

bool fileDialogOpen;

constexpr nfdnfilteritem_t openFilterItem[] = { {L"Game ROM/Save", L"gb,gbc,mbs,sav"} };
constexpr nfdnfilteritem_t saveStateFilterItem[] = { {L"Save State", L"mbs"} };
constexpr nfdnfilteritem_t batterySaveFilterItem[] = { {L"Battery Save", L"sav"} };

const char* errorPopupTitle = "Error Loading the ROM!";
bool errorLoadingROM{false};

bool pauseOnVBlank {false};
extern GBCore gbCore;
std::string FPS_text{ "FPS: 00.00" };

template <typename T>
inline void loadFile(T path)
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
            std::string title = "MegaBoy - " + gbCore.gameTitle;
            glfwSetWindowTitle(window, title.c_str());
            debugUI::clearBuffers();
        }
        }
    }
}

void updateFramebuffer()
{
    if (pauseOnVBlank)
    {
        pauseOnVBlank = false;
        gbCore.options.paused = true;
        gbCore.autoSave();
    }

    {
        std::lock_guard<std::mutex> lock(gbCore.ppu.framebuffer_mutex);
        OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getFrameBuffer());
    }

    std::swap(gbFramebufferTextures[0], gbFramebufferTextures[1]);
    debugUI::updateTextures(gbCore.options.paused);
}

void refreshGBTextures()
{
    std::lock_guard<std::mutex> lock(gbCore.ppu.framebuffer_mutex);
    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getFrameBuffer());
    OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getFrameBuffer());
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

    regularShader.compile("data/shaders/regular_vertex.glsl", "data/shaders/regular_frag.glsl");
    regularShader.use();
    currentShader = &regularShader;

    OpenGL::createTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
    OpenGL::createTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT);

//    gbCore.ppu.drawCallback = drawCallback;
    refreshGBTextures();
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
                nfdresult_t result = NFD::OpenDialog(outPath, openFilterItem, 1, defaultPath.c_str());

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

                    #ifdef _WIN32
                        const std::wstring defaultName = StringUtils::ToUTF16(gbCore.gameTitle.c_str()) + L" - Save State";          
                    #else
                        const std::string defaultName = gbCore.gameTitle + " - Save State";
                    #endif

                    nfdresult_t result = NFD::SaveDialog(outPath, saveStateFilterItem, 1, nullptr, defaultName.c_str());

                    if (result == NFD_OKAY)
                        gbCore.saveState(outPath.get());

                    fileDialogOpen = false;
                }

                if (gbCore.cartridge.ROMLoaded && gbCore.cartridge.hasBattery)
                {
                    if (ImGui::MenuItem("Export Battery"))
                    {
                        fileDialogOpen = true;
                        NFD::UniquePathN outPath;

                        #ifdef _WIN32
                            const std::wstring defaultName = StringUtils::ToUTF16(gbCore.gameTitle.c_str()) + L" - Battery Save";
                        #else
                            const std::string defaultName = gbCore.gameTitle + " - Battery Save";
                        #endif

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
            ImGui::Checkbox("Run Boot ROM", &gbCore.options.runBootROM);
            ImGui::Checkbox("Pause when unfocused", &pauseOnFocus);
            ImGui::Checkbox("Battery Saves", &gbCore.options.batterySaves);

            if (gbCore.cartridge.ROMLoaded && gbCore.getSaveNum() != 0)
            {
                if (gbCore.getSaveNum() != 0)
                    ImGui::Checkbox("Autosave state", &autosaves);
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Graphics"))
        {
            if (ImGui::Checkbox("VSync", &vsync))
                glfwSwapInterval(vsync ? 1 : 0);

            if (!vsync)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Checkbox("FPS Lock", &fpsLock);
            }

            ImGui::SeparatorText("UI");

            if (ImGui::Checkbox("Alpha Blending", &blending))
            {
                if (blending)
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

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static int filter{ 0 };
            constexpr const char* filters[] = { "None", "LCD", "Upscaling" };

            if (ImGui::ListBox("Filter", &filter, filters, 3))
            {
                currentShader = filter == 0 ? &regularShader : filter == 1 ? &lcdShader : &scalingShader;

                if (currentShader->compiled())
                    currentShader->use();
                else
                {
                    switch (filter)
                    {
                    case 1:
                        lcdShader.compile("data/shaders/lcd1x_vertex.glsl", "data/shaders/lcd1x_frag.glsl");
                        lcdShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
                        break;
                    case 2:
                        scalingShader.compile("data/shaders/omniscale_vertex.glsl", "data/shaders/omniscale_frag.glsl");
                        scalingShader.setFloat2("OutputSize", PPU::SCR_WIDTH * 6, PPU::SCR_HEIGHT * 6);
                        scalingShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
                        break;
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static int palette{ 0 };
            constexpr const char* palettes[] = { "BGB Green", "Grayscale", "Classic" };

            if (ImGui::ListBox("Palette", &palette, palettes, 3))
            {
                auto colors = palette == 0 ? PPU::BGB_GREEN_PALETTE : palette == 1 ? PPU::GRAY_PALETTE : PPU::CLASSIC_PALETTE;
                if (gbCore.options.paused || !gbCore.cartridge.ROMLoaded) gbCore.ppu.updateScreenColors(colors);

                gbCore.ppu.setColorsPalette(colors);
                refreshGBTextures();
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation"))
        {
            if (ImGui::MenuItem(gbCore.options.paused ? "Resume" : "Pause", "(Tab)"))
                gbCore.options.paused = !gbCore.options.paused;

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

        debugUI::updateMenu();

        if (gbCore.options.paused)
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

        float text_width = ImGui::CalcTextSize(FPS_text.data()).x;
        float available_width = ImGui::GetContentRegionAvail().x;

        if (text_width < available_width)
        {
            ImGui::SameLine(ImGui::GetWindowWidth() - text_width - ImGui::GetStyle().ItemSpacing.x * 3);
            ImGui::Separator();
            ImGui::Text(FPS_text.data());
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

    if (blending)
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
        pausedPreEvent = gbCore.options.paused;
        gbCore.options.paused = true;
    }
    else
        gbCore.options.paused = pausedPreEvent;
}
void window_focus_callback(GLFWwindow* _window, int focused)
{
    (void)_window;

    if (!pauseOnFocus) return;

    if (!focused)
    {
        pausedPreEvent = gbCore.options.paused;
        gbCore.options.paused = true;
    }
    else
        gbCore.options.paused = pausedPreEvent;
}

void key_callback(GLFWwindow* _window, int key, int scancode, int action, int mods)
{
    (void)_window;

    if (action == 1)
    {
        if (key == GLFW_KEY_TAB)
        {
            if (gbCore.options.paused) gbCore.options.paused = false;
            else
            {
                if (gbCore.cartridge.ROMLoaded) pauseOnVBlank = true;
                else gbCore.options.paused = true;
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
                gbCore.saveState(gbCore.getSaveFolderPath() + "/quicksave.mbs");

            return;
        }
        if (key == GLFW_KEY_GRAVE_ACCENT)
        {
            if (gbCore.cartridge.ROMLoaded)
            {
                std::string statePath = gbCore.getSaveFolderPath() + "/quicksave.mbs";
                loadFile(statePath.c_str());
            }
            return;
        }
    }

    if (!gbCore.options.paused)
        gbCore.input.update(scancode, action);
}

void drop_callback(GLFWwindow* _window, int count, const char** paths)
{
    (void)_window;

    if (count > 0)
    {
    #ifdef _WIN32
        auto utf16 = StringUtils::ToUTF16(paths[0]);
        loadFile(utf16.c_str());
    #else
        loadFile(paths[0]);
    #endif
    }
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
    glfwSwapInterval(1);
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

    glClearColor(PPU::BGB_GREEN_PALETTE[0].R, PPU::BGB_GREEN_PALETTE[0].G, PPU::BGB_GREEN_PALETTE[0].B, 0);
    glClear(GL_COLOR_BUFFER_BIT);

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
    viewport_width = { static_cast<int>(mode->width * 0.4f) };
    viewport_height = { static_cast<int>(viewport_width / (static_cast<float>(PPU::SCR_WIDTH) / PPU::SCR_HEIGHT)) };

    glfwSetWindowSize(window, viewport_width, viewport_height + menuBarHeight);
    glfwSetWindowAspectRatio(window, viewport_width, viewport_height);

    uint16_t maxHeight{ static_cast<uint16_t>(mode->height - mode->height / 15.0) };
    glfwSetWindowSizeLimits(window, PPU::SCR_WIDTH * 2, PPU::SCR_HEIGHT * 2, maxHeight * (PPU::SCR_WIDTH / PPU::SCR_HEIGHT), maxHeight);
    glViewport(0, 0, viewport_width, viewport_height);

    vsyncCPUCycles = GBCore::calculateCycles(1.0 / mode->refreshRate);
}

void setImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "data/imgui.ini";

    const int resolutionX = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
    scaleFactor = (resolutionX / 1920.0f);

    io.Fonts->AddFontFromFileTTF("data/robotomono.ttf", scaleFactor * 18.0f);
    ImGui::GetStyle().ScaleAllSizes(scaleFactor);

    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

int main()
{
    if (!setGLFW()) return -1;
    setImGUI();
    setWindowSize();
    setBuffers();

    double lastFrameTime = glfwGetTime();
    double fpsTimer{};
    int frameCount{};

    while (!glfwWindowShouldClose(window))
    {
        double currentFrameTime = glfwGetTime();
        double deltaTime = currentFrameTime - lastFrameTime;

        fpsTimer += deltaTime;

      //  const bool updateCPU = vsync || timer >= GBCore::FRAME_RATE;  
        //const bool updateRender = updateCPU || (!vsync && !fpsLock);

        //if (updateCPU)
        //{
        //    gbCore.update(vsync ? vsyncCPUCycles : GBCore::CYCLES_PER_FRAME);
        //    timer = 0;
        //}

       // if (updateRender)

        if (gbCore.ppu.drawCallback)
        {
            updateFramebuffer();
            gbCore.ppu.drawCallback = false;
        }

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

            if (!gbCore.options.paused && autosaves) 
                gbCore.autoSave(); // Autosave once a second.
        }

        lastFrameTime = currentFrameTime;
//        std::this_thread::sleep_for(std::chrono::milliseconds(0));

        if (gbCore.options.paused)
            glfwWaitEvents();
    }

    gbCore.saveCurrentROM();
    return 1;
}