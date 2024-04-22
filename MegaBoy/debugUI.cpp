#include "debugUI.h"
#include <ImGUI/imgui.h>
#include "glFunctions.h"

void OAM_Window_renderEvent(const std::array<pixelInfo, PPU::SCR_WIDTH>& updatedPixels, uint8_t LY)
{
    debugUI::clearBGScanline(LY);

    for (int x = 0; x < PPU::SCR_WIDTH; x++)
    {
        if (updatedPixels[x].isSet)
            debugUI::setBGPixel(x, LY, updatedPixels[x].data);
    }
}

void backgroundRenderEvent(const std::array<uint8_t, PPU::FRAMEBUFFER_SIZE>& buffer, uint8_t LY)
{
    for (int x = 0; x < PPU::SCR_WIDTH; x++)
        debugUI::setBGPixel(x, LY, PixelOps::getPixel(buffer.data(), PPU::SCR_WIDTH, x, LY));
}

void debugUI::updateMenu(GBCore& gbCore)
{
    if (ImGui::BeginMenu("Debug"))
    {
        if (ImGui::MenuItem("Memory View"))
        {
            showMemoryView = !showMemoryView;

            if (firstMemoryViewOpen)
            {
                memoryData = std::make_unique<std::string[]>(4096);
                firstMemoryViewOpen = false;
            }
            else
                std::fill(memoryData.get(), memoryData.get() + 4096, "");
        }
        if (ImGui::MenuItem("VRAM View"))
        {
            if (firstVRAMViewOpen)
            {
                OpenGL::createTexture(backgroundTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT);
                BGFrameBuffer = std::make_unique<uint8_t[]>(PPU::FRAMEBUFFER_SIZE);
                clearBGBuffer();
                firstVRAMViewOpen = false;
            }

            showVRAMView = !showVRAMView;

            if (!showVRAMView)
                gbCore.ppu.resetCallbacks();
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

inline void displayImage(uint32_t texture, uint8_t* data, uint16_t width = PPU::SCR_WIDTH, uint16_t height = PPU::SCR_HEIGHT)
{
    OpenGL::updateTexture(texture, width, height, data);
    ImGui::Image((void*)texture, ImGui::GetContentRegionAvail());
}

enum VRAMTab
{
    Background,
    Window,
    OAM,
    TileData
};

VRAMTab currentVramTab;

void debugUI::updateWindows(GBCore& gbCore)
{
    if (showMemoryView)
    {
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
                        memoryData[i] += to_hex_str(gbCore.mmu.read8(i * 16 + j)) + " "; 
                }

                ImGui::Text(memoryData[i].data());;
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

    if (showVRAMView)
    {
        ImGui::Begin("VRAM View", &showVRAMView);
        gbCore.ppu.resetCallbacks();

        if (ImGui::BeginTabBar("tabbar"))
        {
            if (ImGui::BeginTabItem("Background"))
            {
                gbCore.ppu.onBackgroundRender = backgroundRenderEvent;
                if (currentVramTab != VRAMTab::Background) clearBGBuffer();

                displayImage(backgroundTexture, BGFrameBuffer.get());
                ImGui::EndTabItem();
                currentVramTab = VRAMTab::Background;
            }
            if (ImGui::BeginTabItem("Window"))
            {
                gbCore.ppu.onWindowRender = OAM_Window_renderEvent;
                if (currentVramTab != VRAMTab::Window) clearBGBuffer();

                displayImage(backgroundTexture, BGFrameBuffer.get());
                currentVramTab = VRAMTab::Window;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("OAM"))
            {
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
                            oamEntry += to_hex_str(gbCore.mmu.directRead(0xFE00 + i * 16 + j)) + " ";

                        ImGui::Text(oamEntry.data());
                    }
                }
                else
                {
                    gbCore.ppu.onOAMRender = OAM_Window_renderEvent;
                    if (currentVramTab != VRAMTab::OAM) clearBGBuffer();

                    displayImage(backgroundTexture, BGFrameBuffer.get());
                    currentVramTab = VRAMTab::OAM;
                }

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tile Data"))
            {
                if (firstVRAMTileDataOpen)
                {
                    OpenGL::createTexture(tileDataTexture, 32 * 8, 32 * 8);
                    tileDataFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEDATA_FRAMEBUFFER_SIZE);
                    clearTileDataBuffer();
                    firstVRAMTileDataOpen = false;
                }

                gbCore.ppu.renderTileData(tileDataFrameBuffer.get());
                displayImage(tileDataTexture, tileDataFrameBuffer.get(), PPU::TILES_SIZE, PPU::TILES_SIZE);

                currentVramTab = VRAMTab::TileData;
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }
}