#include "debugUI.h"

#include <cmath>
#include <ImGUI/imgui.h>

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
        if (ImGui::MenuItem("CPU View"))
        {
            showCPUView = !showCPUView;
        }
        if (ImGui::MenuItem("VRAM View"))
        {
            showVRAMView = !showVRAMView;
        }
        if (ImGui::MenuItem("Audio View"))
        {
            showAudioView = !showAudioView;
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

inline void displayImage(uint32_t texture, uint16_t width = PPU::TILEMAP_WIDTH, uint16_t height = PPU::TILEMAP_HEIGHT)
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
                if (realTimeMemView || memoryData[i].empty())
                {
                    memoryData[i] = "0x" + to_hex_str(static_cast<uint16_t>(i * 16)) + " ";

                    for (int j = 0; j < 16; j++)
                    {
                        if (gbCore.cartridge.ROMLoaded)
                            memoryData[i] += to_hex_str(gbCore.mmu.read8(static_cast<uint16_t>(i * 16 + j))) + " ";
                        else
                            memoryData[i] += "ff ";
                    }
                }

                ImGui::Text("%s", memoryData[i].c_str());
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }
    if (showCPUView)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(-1.f, -1.f), ImVec2(-1.f, -1.f));
        ImGui::Begin("CPU View", &showCPUView);

        std::string strBuf = "PC: " + to_hex_str(gbCore.cpu.s.PC);

        ImGui::Text("%s", strBuf.c_str());

        strBuf = "SP: " + to_hex_str(gbCore.cpu.s.SP.val);
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "IE: " + to_hex_str(gbCore.cpu.s.IE);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SameLine();

        strBuf = "IF: " + to_hex_str(gbCore.cpu.s.IF);
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "DIV: " + to_hex_str(gbCore.cpu.s.DIV_reg);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SameLine();

        strBuf = "TIMA: " + to_hex_str(gbCore.cpu.s.TIMA_reg);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SeparatorText("Registers");

        strBuf = "AF: " + to_hex_str(gbCore.cpu.registers.AF.val);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SameLine();

        strBuf = "BC: " + to_hex_str(gbCore.cpu.registers.BC.val);
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "DE: " + to_hex_str(gbCore.cpu.registers.DE.val);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SameLine();

        strBuf = "HL: " + to_hex_str(gbCore.cpu.registers.HL.val);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SeparatorText("Flags");

        strBuf = "Zero: " + std::string(gbCore.cpu.registers.getFlag(FlagType::Zero) ? "1" : "0");
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "Carry: " + std::string(gbCore.cpu.registers.getFlag(FlagType::Carry) ? "1" : "0");
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "HalfCarry: " + std::string(gbCore.cpu.registers.getFlag(FlagType::HalfCarry) ? "1" : "0");
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "Negative: " + std::string(gbCore.cpu.registers.getFlag(FlagType::Subtract) ? "1" : "0");
        ImGui::Text("%s", strBuf.c_str());

        ImGui::End();
    }

    if (showVRAMView)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(365.0f * scaleFactor, (365.0f * scaleFactor) + ImGui::GetFrameHeight() * 2), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::Begin("VRAM View", &showVRAMView);

        if (ImGui::BeginTabBar("tabbar"))
        {
            if (ImGui::BeginTabItem("Tile Data"))
            {
                if (System::Current() == GBSystem::GBC)
                {
                    ImGui::RadioButton("VRAM Bank 0", &vramTileBank, 0);
                    ImGui::SameLine();
                    ImGui::RadioButton("VRAM Bank 1", &vramTileBank, 1);
                }

                if (!tileDataTexture)
                {
                    tileDataFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEDATA_FRAMEBUFFER_SIZE);
                    clearBuffer(tileDataFrameBuffer.get(), PPU::TILES_WIDTH, PPU::TILES_HEIGHT);
                    OpenGL::createTexture(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileDataFrameBuffer.get());
                }

                if (gbCore.cartridge.ROMLoaded)
                {
                    gbCore.ppu->renderTileData(tileDataFrameBuffer.get(), System::Current() == GBSystem::DMG ? 0 : vramTileBank);
                    OpenGL::updateTexture(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileDataFrameBuffer.get());
                }

                displayImage(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Background Map"))
            {
                if (!backgroundTexture)
                {
                    BGFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEMAP_FRAMEBUFFER_SIZE);
                    clearBuffer(BGFrameBuffer.get(), PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT);
                    OpenGL::createTexture(backgroundTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, BGFrameBuffer.get());
                }

                if (gbCore.cartridge.ROMLoaded)
				{
					gbCore.ppu->renderBGTileMap(BGFrameBuffer.get());
					OpenGL::updateTexture(backgroundTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, BGFrameBuffer.get());
				}

                displayImage(backgroundTexture);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Window Map"))
            {
                if (!windowTexture)
                {
                    windowFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEMAP_FRAMEBUFFER_SIZE);
                    clearBuffer(windowFrameBuffer.get(), PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT);
                    OpenGL::createTexture(windowTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, windowFrameBuffer.get());
                }

                if (gbCore.cartridge.ROMLoaded)
                {
                    gbCore.ppu->renderWindowTileMap(windowFrameBuffer.get());
                    OpenGL::updateTexture(windowTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, windowFrameBuffer.get());
                }

                displayImage(windowTexture);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    if (showAudioView)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(-1.f, -1.f), ImVec2(INFINITY, INFINITY));
        ImGui::Begin("Audio", &showAudioView);

        ImGui::SeparatorText("Channel Options");

        ImGui::Checkbox("Channel 1", &gbCore.apu.enableChannel1);
        ImGui::Checkbox("Channel 2", &gbCore.apu.enableChannel2);
        ImGui::Checkbox("Channel 3", &gbCore.apu.enableChannel3);
        ImGui::Checkbox("Channel 4", &gbCore.apu.enableChannel4);

        ImGui::End();
    }
}