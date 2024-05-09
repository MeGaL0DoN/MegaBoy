#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "nfd/nfd.hpp"

#include <iostream>
#include <filesystem>
#include <thread>

#include "GBCore.h"
#include "Shader.h"
#include "debugUI.h"
#include "glFunctions.h"

GLFWwindow* window;
bool saveManagerOpen{ false };

bool blending{ true };
bool pauseOnMinimize { true };
bool autoSaves { true };

bool fpsLock{ true };
bool vsync { true };
int vsyncCPUCycles;

int menuBarHeight;
int viewport_width, viewport_height;
float scaleFactor;

Shader regularShader;
Shader scalingShader;
std::array<uint32_t, 2> gbFramebufferTextures;

const std::wstring defaultPath{ std::filesystem::current_path().wstring() };
constexpr nfdnfilteritem_t filterItem[] = { {L"Game ROM", L"gb,bin"} };
bool fileDialogOpen;

constexpr const char* errorPopupTitle = "Error Loading the ROM!";
bool errorLoadingROM{false};

bool pauseOnVBlank {false};
extern GBCore gbCore;
std::string FPS_text{ "FPS: 00.00" };

template <typename T>
bool loadBase(T path)
{
    if (!gbCore.cartridge.loadROM(path))
    {
        errorLoadingROM = true;
        return false;
    }

    std::string title = "MegaBoy - " + gbCore.gameTitle;
    glfwSetWindowTitle(window, title.c_str());

    debugUI::clearBuffers();
    return true;
}

std::wstring currentROMPAth{};
inline void loadROM(const wchar_t* path)
{
    if (std::filesystem::exists(path))
    {
        if (loadBase(path))
            currentROMPAth = path;
    }
}
inline void loadROM(const char* path)
{
    if (std::filesystem::exists(path))
    {
        if (loadBase(path))
            currentROMPAth = std::wstring(path, path + strlen(path));
    }
}

void drawCallback(const uint8_t* framebuffer)
{
    if (pauseOnVBlank)
    {
        pauseOnVBlank = false;
        gbCore.paused = true;
    }

    //glBindTexture(GL_TEXTURE_2D, gbFramebufferTextures[0]);
    //glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, 0);

    OpenGL::updateTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuffer);

    auto temp = gbFramebufferTextures[0];
    gbFramebufferTextures[0] = gbFramebufferTextures[1];
    gbFramebufferTextures[1] = temp;

  //  OpenGL::updateTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT, framebuffer);
    debugUI::updateTextures(gbCore.paused);
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

    OpenGL::createTexture(gbFramebufferTextures[0], PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
    OpenGL::createTexture(gbFramebufferTextures[1], PPU::SCR_WIDTH, PPU::SCR_HEIGHT);

    gbCore.ppu.drawCallback = drawCallback;
    gbCore.ppu.invokeDrawCallback();
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
            if (ImGui::MenuItem("Load Game"))
            {
                fileDialogOpen = true;
                NFD::UniquePathN outPath;
                nfdresult_t result = NFD::OpenDialog(outPath, filterItem, 1, defaultPath.c_str());

                if (result == NFD_OKAY)
                    loadROM(outPath.get());

                fileDialogOpen = false;
            }

            if (ImGui::MenuItem("Save Manager"))
                saveManagerOpen = !saveManagerOpen;

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings", "Ctrl+Q"))
        {
            ImGui::Checkbox("Auto Saves", &autoSaves);
            ImGui::Checkbox("Run Boot ROM", &gbCore.runBootROM);
            ImGui::Checkbox("Pause when minimized", &pauseOnMinimize);

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Graphics"))
        {
            static bool upscaling{ false };

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
                if (blending) glEnable(GL_BLEND);
                else
                {
                    glDisable(GL_BLEND);
                    regularShader.setFloat("alpha", 1.0f);
                }
            }

            if (ImGui::Checkbox("Upscaling Filter", &upscaling))
            {
                if (upscaling)
                {
                    if (scalingShader.compiled())
                        scalingShader.use();
                    else
                    {
                        scalingShader.compile("data/shaders/omniscale_vertex.glsl", "data/shaders/omniscale_frag.glsl");
                        scalingShader.use();

                        scalingShader.setFloat2("OutputSize", PPU::SCR_WIDTH * 6, PPU::SCR_HEIGHT * 6); // 6x seems to be the best scale
                        scalingShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
                    }
                }
                else
                    regularShader.use();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static int palette{ 0 };
            constexpr const char* palettes[] = { "BGB Green", "Grayscale", "Classic" };

            if (ImGui::ListBox("Palette", &palette, palettes, 3))
            {
                auto colors = palette == 0 ? PPU::BGB_GREEN_PALETTE : palette == 1 ? PPU::GRAY_PALETTE : PPU::CLASSIC_PALETTE;
                if (gbCore.paused || !gbCore.cartridge.ROMLoaded) gbCore.ppu.updateScreenColors(colors);

                gbCore.ppu.setColorsPalette(colors);
                gbCore.ppu.invokeDrawCallback();
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation"))
        {
            if (ImGui::MenuItem(gbCore.paused ? "Resume" : "Pause", "(Tab)"))
                gbCore.paused = !gbCore.paused;

            if (ImGui::MenuItem("Reload", "(Esc)"))
                loadROM(currentROMPAth.c_str());

            ImGui::EndMenu();
        }

        debugUI::updateMenu();

        if (gbCore.paused)
        {
            ImGui::Separator();
            ImGui::Text("Emulation Paused");
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

    if (saveManagerOpen)
    {
        // TODO
    }

    if (errorLoadingROM)
    {
        ImGui::OpenPopup(errorPopupTitle);
        errorLoadingROM = false;
    }

    if (ImGui::BeginPopupModal(errorPopupTitle, 0, ImGuiWindowFlags_NoMove))
    {
        float windowWidth = ImGui::GetWindowSize().x;
        ImGui::SetCursorPosX((windowWidth - (75 * scaleFactor)) * 0.5f);

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
        regularShader.setFloat("alpha", 1.0f);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        regularShader.setFloat("alpha", 0.5f);
        OpenGL::bindTexture(gbFramebufferTextures[1]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    else
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);
    renderGameBoy();
    renderImGUI();
    glfwSwapBuffers(window);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    viewport_width = width; viewport_height = height - menuBarHeight;
    glViewport(0, 0, viewport_width, viewport_height);
}

void window_refresh_callback(GLFWwindow* window)
{
    if (!fileDialogOpen) render(); // Super strange issue - ImGUI crashes if rendering is done while file dialog is open???
}

bool pausedPreMinimize;
void window_iconify_callback(GLFWwindow* window, int iconified)
{
    if (!pauseOnMinimize) return;

    if (iconified)
    {
        pausedPreMinimize = gbCore.paused;
        gbCore.paused = true;
    }
    else
        gbCore.paused = pausedPreMinimize;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == 1)
    {
        if (key == GLFW_KEY_ESCAPE)
        {
            gbCore.loadState();
          //  loadROM(currentROMPAth.c_str());
            return;
        }
        if (key == GLFW_KEY_TAB)
        {
            gbCore.saveState(); 
            //if (gbCore.paused) gbCore.paused = false;
            //else
            //{
            //    if (gbCore.cartridge.ROMLoaded) pauseOnVBlank = true;
            //    else gbCore.paused = true;
            //}
            //return;
        }
    }

    if (!gbCore.paused)
        gbCore.input.update(scancode, action);
}
void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    if (count > 0)
        loadROM(paths[0]);
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    uint16_t maxHeight = mode->height - static_cast<int16_t>(mode->height / 15.0);
    glfwSetWindowSizeLimits(window, PPU::SCR_WIDTH * 2, PPU::SCR_HEIGHT * 2, maxHeight * (PPU::SCR_WIDTH / PPU::SCR_HEIGHT), maxHeight);
    glViewport(0, 0, viewport_width, viewport_height);

    vsyncCPUCycles = GBCore::getCycles(1.0 / mode->refreshRate);
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
    double timer{};
    double fpsTimer{};
    int frameCount{};

    while (!glfwWindowShouldClose(window))
    {
        double currentFrameTime = glfwGetTime();
        double deltaTime = currentFrameTime - lastFrameTime;

        timer += deltaTime;
        fpsTimer += deltaTime;

        const bool updateCPU = vsync || timer >= GBCore::FRAME_RATE;  
        const bool updateRender = updateCPU || (!vsync && !fpsLock);

        if (updateCPU)
        {
            gbCore.update(vsync ? vsyncCPUCycles : GBCore::CYCLES_PER_FRAME);
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

           // if (gbCore.cartridge.ROMLoaded) // Autosave once a second.
             //   gbCore.saveState(); //cartridge.saveGame();
        }

        lastFrameTime = currentFrameTime;
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

        if (gbCore.paused)
            glfwWaitEvents();
    }

    return 1;
}