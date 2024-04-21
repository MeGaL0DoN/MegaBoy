#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "nfd/nfd.hpp"

#include <iostream>
#include <thread>
#include <filesystem>

#include "GBCore.h"
#include "Shader.h"
#include "debugUI.h"
#include "glFunctions.h"

GLFWwindow* window;
int menuBarHeight;
int viewport_width, viewport_height;

Shader regularShader;
Shader scalingShader;
bool scalingShaderCompiled{ false };

uint32_t gbFramebufferTexture;

const std::wstring defaultPath{ std::filesystem::current_path().wstring() };
constexpr nfdnfilteritem_t filterItem[] = { {L"Game ROM", L"gb,bin"} };

GBCore gbCore{};
std::string FPS_text{ "FPS: 00.00" };

std::wstring currentROMPAth{};
void loadROM(const wchar_t* path)
{
    if (std::filesystem::exists(path))
    {
        gbCore.mmu.loadROM(path);
        currentROMPAth = path;
    }
}
void loadROM(const char* path)
{
    if (std::filesystem::exists(path))
    {
        gbCore.mmu.loadROM(path);
        currentROMPAth = std::wstring(path, path + strlen(path));
    }
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

    OpenGL::createTexture(gbFramebufferTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT);

    regularShader.compile("data/shaders/regular_vertex.glsl", "data/shaders/regular_frag.glsl");
    regularShader.use();
}

void renderGameBoy()
{
    OpenGL::updateTexture(gbFramebufferTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getFrameBuffer());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
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
                NFD::UniquePathN outPath;
                nfdresult_t result = NFD::OpenDialog(outPath, filterItem, 1, defaultPath.c_str());

                if (result == NFD_OKAY)
                {
                    loadROM(outPath.get());
                    outPath.release();
                }
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings", "Ctrl+Q"))
        {
            ImGui::Checkbox("Run Boot ROM", &gbCore.runBootROM);

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Graphics"))
        {
            static bool vsync{ true };
            static bool upscaling{ false };

            if (ImGui::Checkbox("VSync", &vsync))
                glfwSwapInterval(vsync ? 1 : 0);

            ImGui::SeparatorText("UI");

            if (ImGui::Checkbox("Upscaling Filter", &upscaling))
            {
                if (upscaling)
                {
                    if (scalingShaderCompiled)
                        scalingShader.use();
                    else
                    {
                        scalingShader.compile("data/shaders/omniscale_vertex.glsl", "data/shaders/omniscale_frag.glsl");
                        scalingShader.use();

                        scalingShader.setFloat2("OutputSize", PPU::SCR_WIDTH * 6, PPU::SCR_HEIGHT * 6); // 6x seems to be the best scale
                        scalingShader.setFloat2("TextureSize", PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
                        scalingShaderCompiled = true;
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
                if (gbCore.paused || !gbCore.mmu.ROMLoaded) gbCore.ppu.updateScreenColors(colors);
                gbCore.ppu.setColorsPalette(colors);
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

        debugUI::updateMenu(gbCore);

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

    debugUI::updateWindows(gbCore);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    updateImGUIViewports();
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
    render();
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == 1)
    {
        if (key == GLFW_KEY_ESCAPE)
        {
            loadROM(currentROMPAth.c_str());
            return;
        }
        if (key == GLFW_KEY_TAB)
        {
            gbCore.paused = !gbCore.paused;
            return;
        }
    }

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
    glViewport(0, 0, viewport_width, viewport_height);
}

void setImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "data/imgui.ini";

    const int resolutionX = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
    io.Fonts->AddFontFromFileTTF("data/robotomono.ttf", (resolutionX / 1920.0f) * 18.0f);

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

        constexpr double maxDeltaTime = 1.0 / 5.0;
        if (deltaTime < maxDeltaTime) gbCore.update(deltaTime); // So holding the window down and then releasing doesn't block emulator by executing a bunch of instructions... 

        glfwPollEvents();
        render();

        if (fpsTimer >= 1.0)
        {
            double fps = frameCount / fpsTimer;
            FPS_text = "FPS: " + std::format("{:.2f}", fps);

            frameCount = 0;
            fpsTimer = 0;
        }

        frameCount++;
        lastFrameTime = currentFrameTime;
    }

    return 1;
}