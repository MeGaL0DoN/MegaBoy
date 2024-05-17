#include "debugUI.h"
#include <ImGUI/imgui.h>

void debugUI::backgroundRenderEvent(const uint8_t* buffer, uint8_t LY)
{
    if (!BGFrameBuffer) return;

    for (uint8_t x = 0; x < PPU::SCR_WIDTH; x++)
        PixelOps::setPixel(BGFrameBuffer.get(), PPU::SCR_WIDTH, x, LY, PixelOps::getPixel(buffer, PPU::SCR_WIDTH, x, LY));
}

void debugUI::OAM_renderEvent(const uint8_t* buffer, const std::vector<uint8_t>& updatedPixels, uint8_t LY)
{
    if (!OAMFrameBuffer) return;
    clearBGScanline(OAMFrameBuffer.get(), LY);

    for (int i = 0; i < updatedPixels.size(); i++)
    {
        uint8_t x = updatedPixels[i];
        PixelOps::setPixel(OAMFrameBuffer.get(), PPU::SCR_WIDTH, x, LY, PixelOps::getPixel(buffer, PPU::SCR_WIDTH, x, LY));
    }
}
void debugUI::windowRenderEvent(const uint8_t* buffer, const std::vector<uint8_t>& updatedPixels, uint8_t LY)
{
    if (!windowFrameBuffer) return;
    clearBGScanline(windowFrameBuffer.get(), LY);

    for (int i = 0; i < updatedPixels.size(); i++)
    {
        uint8_t x = updatedPixels[i];
        PixelOps::setPixel(windowFrameBuffer.get(), PPU::SCR_WIDTH, x, LY, PixelOps::getPixel(buffer, PPU::SCR_WIDTH, x, LY));
    }
}

void debugUI::updateMenu()
{
    if (ImGui::BeginMenu("Debug"))
    {
        if (ImGui::MenuItem("Memory View"))
        {
            showMemoryView = !showMemoryView;

            if (memoryData == nullptr)
                memoryData = std::make_unique<std::string[]>(4096);
            else
                std::fill(memoryData.get(), memoryData.get() + 4096, "");
        }
        if (ImGui::MenuItem("VRAM View"))
        {
            showVRAMView = !showVRAMView;

            if (showVRAMView)
            {
                gbCore.ppu.onBackgroundRender = debugUI::backgroundRenderEvent;
                gbCore.ppu.onWindowRender = debugUI::windowRenderEvent;
                gbCore.ppu.onOAMRender = debugUI::OAM_renderEvent;
            }
            else
                gbCore.ppu.resetRenderCallbacks();
        }

        ImGui::EndMenu();
    }
}

inline std::string to_hex_str(uint8_t i) 
{
    constexpr std::array<char, 16> hex_chars = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    std::string hex_str(2, '0');

    hex_str[0] = hex_chars[(i & 0xF0) >> 4];
    hex_str[1] = hex_chars[i & 0x0F];

    return hex_str;
}
inline std::string to_hex_str(uint16_t i) 
{
    constexpr std::array<char, 16> hex_chars = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    std::string hex_str(4, '0');

    hex_str[0] = hex_chars[(i & 0xF000) >> 12];
    hex_str[1] = hex_chars[(i & 0x0F00) >> 8];
    hex_str[2] = hex_chars[(i & 0x00F0) >> 4];
    hex_str[3] = hex_chars[i & 0x000F];

    return hex_str;
}

inline void displayMemHeader()
{
    constexpr const char* header = "Offset 00 01 02 03 04 04 06 07 08 09 0A 0B 0C 0D 0E 0F  ";
    ImGui::Text(header);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

inline void displayImage(uint32_t texture, uint16_t width = PPU::SCR_WIDTH, uint16_t height = PPU::SCR_HEIGHT)
{
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();
    const ImVec2 contentPos = ImGui::GetCursorScreenPos();

    const float aspectRatio = static_cast<float>(width) / height;
    const float aspectRatioContent = contentSize.x / contentSize.y;

    ImVec2 imageSize, imagePos;

    if (aspectRatioContent > aspectRatio)
    {
        imageSize.x = contentSize.y * aspectRatio;
        imageSize.y = contentSize.y;
    }
    else
    {
        imageSize.x = contentSize.x;
        imageSize.y = contentSize.x / aspectRatio;
    }

    imagePos.x = contentPos.x + (contentSize.x - imageSize.x) * 0.5f;
    imagePos.y = contentPos.y + (contentSize.y - imageSize.y) * 0.5f;

    ImGui::SetCursorScreenPos(imagePos);
    OpenGL::bindTexture(texture);

    uint64_t _texture64 { texture };
    ImGui::Image(reinterpret_cast<void*>(_texture64), imageSize);
}

enum class VRAMTab
{
    Background,
    Window,
    OAM,
    TileData
};

VRAMTab currentTab;

void debugUI::updateTextures(bool forceUpdate)
{
    if (showVRAMView)
    {
        switch (forceUpdate ? VRAMTab::Background : currentTab)
        {
        case VRAMTab::Background:
            OpenGL::updateTexture(backgroundTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, BGFrameBuffer.get());
            if (!forceUpdate) break;
        case VRAMTab::Window:
            OpenGL::updateTexture(windowTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, windowFrameBuffer.get());
            if (!forceUpdate) break;
        case VRAMTab::OAM:
            OpenGL::updateTexture(OAMTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, OAMFrameBuffer.get());
            if (!forceUpdate) break;
        case VRAMTab::TileData:
            gbCore.ppu.renderTileData(tileDataFrameBuffer.get());
            OpenGL::updateTexture(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileDataFrameBuffer.get());
            break;
        }
    }
}

void debugUI::updateWindows(float scaleFactor)
{
    if (showMemoryView)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(-1.f, ImGui::GetFontSize() * 19.2f), ImVec2(INFINITY, INFINITY));
        ImGui::Begin("Memory View", &showMemoryView);

        ImGui::BeginDisabled(realTimeMemView);

        if (ImGui::Button("Refresh"))
            std::fill(memoryData.get(), memoryData.get() + 4096, "");

        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::Checkbox("Auto Refresh", &realTimeMemView);
        ImGui::Spacing();
        displayMemHeader();

        ImGui::BeginChild("content");
        ImGuiListClipper clipper;
        clipper.Begin(4096);

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                if (realTimeMemView || memoryData[i] == "")
                {
                    memoryData[i] = "0x" + to_hex_str(static_cast<uint16_t>(i * 16)) + " ";

                    for (int j = 0; j < 16; j++)
                        memoryData[i] += to_hex_str(gbCore.mmu.read8(static_cast<uint16_t>(i * 16 + j))) + " "; 
                }

                ImGui::Text(memoryData[i].data());;
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

    if (showVRAMView)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(460.0f * scaleFactor, (392.0f * scaleFactor) + ImGui::GetFrameHeight() * 2), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::Begin("VRAM View", &showVRAMView);

        if (ImGui::BeginTabBar("tabbar"))
        {
            if (ImGui::BeginTabItem("Background"))
            {
                currentTab = VRAMTab::Background;

                if (!backgroundTexture)
                {
                    BGFrameBuffer = std::make_unique<uint8_t[]>(PPU::FRAMEBUFFER_SIZE);
                    clearBGBuffer(BGFrameBuffer.get());
                    OpenGL::createTexture(backgroundTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, BGFrameBuffer.get());
                }

                displayImage(backgroundTexture);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Window"))
            {
                currentTab = VRAMTab::Window;

                if (!windowTexture)
                {
                    windowFrameBuffer = std::make_unique<uint8_t[]>(PPU::FRAMEBUFFER_SIZE);
                    clearBGBuffer(windowFrameBuffer.get());
                    OpenGL::createTexture(windowTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, windowFrameBuffer.get());
                }

                displayImage(windowTexture);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("OAM"))
            {
                currentTab = VRAMTab::OAM;

                static bool showOAMMem {false};
                ImGui::Checkbox("MEM View", &showOAMMem);
                ImGui::Spacing();

                if (showOAMMem)
                {
                    displayMemHeader();

                    for (int i = 0; i < 10; i++)
                    {
                        std::string oamEntry = "0x" + to_hex_str(static_cast<uint16_t>(0xFE00 + i * 16)) + " ";

                        for (int j = 0; j < 16; j++)
                            oamEntry += to_hex_str(gbCore.ppu.getOAM()[i * 16 + j]) + " ";

                        ImGui::Text(oamEntry.data());
                    }
                }
                else
                {
                    if (!OAMTexture)
                    {
                        OAMFrameBuffer = std::make_unique<uint8_t[]>(PPU::FRAMEBUFFER_SIZE);
                        clearBGBuffer(OAMFrameBuffer.get());
                        OpenGL::createTexture(OAMTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, OAMFrameBuffer.get());
                    }

                    displayImage(OAMTexture);
                }

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tile Data"))
            {
                currentTab = VRAMTab::TileData;

                if (!tileDataTexture)
                {
                    tileDataFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEDATA_FRAMEBUFFER_SIZE);
                    clearTileDataBuffer();
                    OpenGL::createTexture(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileDataFrameBuffer.get());
                }

                displayImage(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }
}