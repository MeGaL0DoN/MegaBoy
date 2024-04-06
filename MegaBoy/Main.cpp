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

GLFWwindow* window;
int menuBarHeight;
int viewport_width, viewport_height;

unsigned int texture;

const std::wstring defaultPath{ std::filesystem::current_path().wstring() };
constexpr nfdnfilteritem_t filterItem[] = { {L"Game ROM", L"gb,bin"} };

GBCore gbCore{};

std::wstring currentROMPAth{};
void loadROM(const wchar_t* path)
{
    system("cls");
    gbCore.mmu.loadROM(path);
    currentROMPAth = path;
}
void loadROM(const char* path)
{
    system("cls");
    gbCore.mmu.loadROM(path);
    currentROMPAth = std::wstring(path, path + strlen(path));
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

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    Shader textureShader { "data/Shaders/vertexShader.glsl", "data/Shaders/fragmentShader.glsl" };
    textureShader.use();
    glClearColor(255, 255, 255, 0);
}

void renderGameBoy()
{
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, gbCore.ppu.getRenderingBuffer());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
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
                    loadROM(outPath.get());
            }
            else if (ImGui::MenuItem("Reload Game", "(Esc)"))
                loadROM(currentROMPAth.c_str());

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings", "Ctrl+Q"))
        {
            ImGui::EndMenu();
        }
        if (gbCore.paused)
        {
            ImGui::Separator();
            ImGui::Text("Emulation Paused");
        }

        ImGui::EndMainMenuBar();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
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

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    viewport_width = { static_cast<int>(mode->width * 0.4f) };
    viewport_height = { static_cast<int>(viewport_width / (static_cast<float>(PPU::SCR_WIDTH) / PPU::SCR_HEIGHT)) };

    glfwSetWindowSize(window, viewport_width, viewport_height + menuBarHeight);
    glfwSetWindowAspectRatio(window, viewport_width, viewport_height + menuBarHeight);
    glViewport(0, 0, viewport_width, viewport_height);
}

void setImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "data/imgui.ini";

    const int resolutionX = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
    io.Fonts->AddFontFromFileTTF("data/roboto.ttf", (resolutionX / 1920.0f) * 17);

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

    while (!glfwWindowShouldClose(window))
    {
        double currentFrameTime = glfwGetTime();
        double deltaTime = currentFrameTime - lastFrameTime;
        timer += deltaTime;

        if (timer >= GBCore::FRAME_RATE)
        {
            timer = 0;
            gbCore.update();
            glfwPollEvents();
            render();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        lastFrameTime = currentFrameTime;
    }

	return 1;
}