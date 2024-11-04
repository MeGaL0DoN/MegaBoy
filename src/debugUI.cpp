#include "debugUI.h"

#include <algorithm>
#include <cmath>
#include <ImGUI/imgui.h>

void debugUI::updateMenu()
{
    if (ImGui::BeginMenu("Debug"))
    {
        if (ImGui::MenuItem("Memory View"))
        {
            showMemoryView = !showMemoryView;
        }
        if (ImGui::MenuItem("CPU View"))
        {
            showCPUView = !showCPUView;
        }
        if (ImGui::MenuItem("Disassembly"))
        {
            showDisassembly = !showDisassembly;
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

void debugUI::disassembleRom()
{
    uint8_t instrLen;
    uint16_t addr = (dissasmRomBank == 0) ? 0 : 0x4000;  
    uint16_t endAddr = addr + 0x3FFF; 

    while (addr <= endAddr)
    {
        std::string disasm = to_hex_str(addr).append(": ").append(gbCore.cpu.disassemble(addr, [](uint16_t a) 
        {
            return gbCore.cartridge.getRom()[(dissasmRomBank * 0x4000) + (a - (dissasmRomBank == 0 ? 0 : 0x4000))];
        }, &instrLen));

        romDisassembly.push_back(disasm);
        addr += instrLen;
    }
}


inline void displayMemHeader()
{
    constexpr const char* header = "Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ";
    ImGui::Text(header);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void debugUI::signalVBlank()
{
    if (showVRAMView)
    {
        switch (currentTab)
        {
        case VRAMTab::TileData:
            gbCore.ppu->renderTileData(tileDataFrameBuffer.get(), System::Current() == GBSystem::DMG ? 0 : vramTileBank);
            OpenGL::updateTexture(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileDataFrameBuffer.get());
            break;
        case VRAMTab::BackgroundMap:
            gbCore.ppu->renderBGTileMap(BGFrameBuffer.get());
            OpenGL::updateTexture(backgroundTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, BGFrameBuffer.get());
            break;
        case VRAMTab::WindowMap:
            gbCore.ppu->renderWindowTileMap(windowFrameBuffer.get());
            OpenGL::updateTexture(windowTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, windowFrameBuffer.get());
            break;
        case VRAMTab::OAM:
            OpenGL::updateTexture(oamTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->oamFramebuffer());
            PixelOps::clearBuffer(gbCore.ppu->oamFramebuffer(), PPU::SCR_WIDTH, PPU::SCR_HEIGHT, color{ 0, 0, 0 });
            break;
        }
	}
}

inline void displayImage(uint32_t texture, uint16_t width, uint16_t height, int& scale)
{
    float windowWidth = ImGui::GetWindowWidth();
    float textWidth = ImGui::CalcTextSize("Scale: 00").x + ImGui::GetFrameHeight() * 2 + ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);

    if (ImGui::ArrowButton("##left", ImGuiDir_Left))
        scale = std::max(1, scale - 1);

    ImGui::SameLine();
    ImGui::Text("Scale: %d", scale);
    ImGui::SameLine();

    if (ImGui::ArrowButton("##right", ImGuiDir_Right))
        scale++;

    ImVec2 imageSize{ static_cast<float>(width * scale), static_cast<float>(height * scale) };
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    float xPos = (contentRegion.x - imageSize.x) * 0.5f;

    if (xPos > 0)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xPos);

    OpenGL::bindTexture(texture);
    uint64_t _texture64{ texture };
    ImGui::Image(reinterpret_cast<void*>(_texture64), imageSize);
}

inline std::string disassemble(uint16_t addr, uint8_t* instrLen)
{
    return to_hex_str(static_cast<uint16_t>(addr)).append(": ").append(gbCore.cpu.disassemble(addr, [](uint16_t addr) { return gbCore.mmu.read8(addr); }, instrLen));
}

void debugUI::signalROMLoaded()
{
    romDisassembly.clear();
    dissasmRomBank = 0;
    removeTempBreakpoint();
    showBreakpointHitWindow = false;
}

void debugUI::extendBreakpointDisasmWindow()
{
    shouldScrollToPC = true;

    uint16_t addr = gbCore.cpu.s.PC;
	uint8_t instrLen;
    std::string disasm = disassemble(addr, &instrLen);

    auto addDissasembly = [&addr, &instrLen, &disasm]()
	{
        std::array<uint8_t, 3> data{ 0, 0, 0 };

        for (int i = 0; i < instrLen; i++)
            data[i] = gbCore.mmu.read8(addr + i);

        breakpointDisassembly.push_back(instructionHistoryEntry{ addr, instrLen, data, disasm });
	};

    auto currentInstr = std::find_if(breakpointDisassembly.begin(), breakpointDisassembly.end(), [instrLen, addr](const instructionHistoryEntry& instr) 
    {
        if (instr.addr != addr || instr.length != instrLen) return false;

        for (int i = 0; i < instrLen; i++)
        {
            if (instr.data[i] != gbCore.mmu.read8(instr.addr + i))
                return false;
        }
        return true;
    });

    if (currentInstr != breakpointDisassembly.end())
    {
        breakpointDisasmLine = std::distance(breakpointDisassembly.begin(), currentInstr);

        if (breakpointDisasmLine == breakpointDisassembly.size() - 1)
        {
            addr += instrLen;
            disasm = disassemble(addr, &instrLen);
            addDissasembly();
        }
    }
    else
    {
        addDissasembly();
        addr += instrLen;
        disasm = disassemble(addr, &instrLen);
        addDissasembly();
        breakpointDisasmLine = breakpointDisassembly.size() - 2;
    }
}

void debugUI::removeTempBreakpoint()
{
    if (tempBreakpointAddr != -1)
    {
        if (std::find(breakpoints.begin(), breakpoints.end(), tempBreakpointAddr) == breakpoints.end())
            gbCore.breakpoints[tempBreakpointAddr] = false;

        tempBreakpointAddr = -1;
    }
}

void debugUI::signalBreakpoint()
{
    if (tempBreakpointAddr != -1)
        removeTempBreakpoint();
    else
    {
        showBreakpointHitWindow = true;
        breakpointDisassembly.clear();
    }

    extendBreakpointDisasmWindow();
}

void debugUI::updateWindows(float scaleFactor)
{
    if (showMemoryView)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(-1.f, ImGui::GetFontSize() * 19.2f), ImVec2(INFINITY, INFINITY));
        ImGui::Begin("Memory View", &showMemoryView);

        displayMemHeader();

        ImGui::BeginChild("content");
        ImGuiListClipper clipper;
        clipper.Begin(4096);

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                std::string memoryData(55, '\0');
                memoryData = "0x" + to_hex_str(static_cast<uint16_t>(i * 16)) + " ";

                if (gbCore.cartridge.ROMLoaded)
                {
                    for (int j = 0; j < 16; j++)
                        memoryData += to_hex_str(gbCore.mmu.read8(static_cast<uint16_t>(i * 16 + j))) + " ";
                }
                else
                    memoryData += "ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff";

                ImGui::Text("%s", memoryData.c_str());
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }
    if (showCPUView)
    {
        ImGui::Begin("CPU View", &showCPUView, ImGuiWindowFlags_NoResize);

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

        strBuf = "Cycles: " + std::to_string(gbCore.totalCycles());
        ImGui::Text("%s", strBuf.c_str());

        strBuf = gbCore.cpu.s.GBCdoubleSpeed ? "Frequency: 2.097 MHz" : "Frequency: 1.048 MHz";
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SeparatorText("Registers");

        strBuf = "A: " + to_hex_str(gbCore.cpu.registers.A.val);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SameLine();

        strBuf = "F: " + to_hex_str(gbCore.cpu.registers.F.val);
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "B: " + to_hex_str(gbCore.cpu.registers.B.val);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SameLine();

        strBuf = "C: " + to_hex_str(gbCore.cpu.registers.C.val);
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "D: " + to_hex_str(gbCore.cpu.registers.D.val);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SameLine();

        strBuf = "E: " + to_hex_str(gbCore.cpu.registers.E.val);
        ImGui::Text("%s", strBuf.c_str());

        strBuf = "H: " + to_hex_str(gbCore.cpu.registers.H.val);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SameLine();

        strBuf = "L: " + to_hex_str(gbCore.cpu.registers.L.val);
        ImGui::Text("%s", strBuf.c_str());

        ImGui::SeparatorText("Flags");

        bool zero = gbCore.cpu.registers.getFlag(FlagType::Zero);
        bool carry = gbCore.cpu.registers.getFlag(FlagType::Carry);
        bool halfCarry = gbCore.cpu.registers.getFlag(FlagType::HalfCarry);
        bool negative = gbCore.cpu.registers.getFlag(FlagType::Subtract);

        ImGui::BeginDisabled();

        ImGui::Checkbox("Zero", &zero);
        ImGui::Checkbox("Carry", &carry);
        ImGui::Checkbox("Half Carry", &halfCarry);
        ImGui::Checkbox("Negative", &negative);

        ImGui::EndDisabled();

        ImGui::End();
    }

    if (showDisassembly)
    {
        ImGui::Begin("Disassembly", &showDisassembly);

        if (showBreakpointHitWindow)
            ImGui::Columns(2);

        ImGui::SeparatorText("Disassembly View");

        ImGui::RadioButton("Memory Space", &romDisassemblyView, 0);
        ImGui::SameLine();
        ImGui::RadioButton("ROM Banks", &romDisassemblyView, 1);

        if (!romDisassemblyView)
        {
            static int breakpointAddr{};
            ImGui::SeparatorText("Breakpoint Address");

            if (ImGui::InputInt("##addr", &breakpointAddr, 0, 0, ImGuiInputTextFlags_CharsHexadecimal))
                breakpointAddr = std::clamp(breakpointAddr, 0, 0xFFFF);

            ImGui::SameLine();

            if (!gbCore.breakpoints[breakpointAddr])
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));

                if (ImGui::Button("Add"))
                {
                    breakpoints.push_back(static_cast<uint16_t>(breakpointAddr));
                    gbCore.breakpoints[breakpointAddr] = true;
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));

                if (ImGui::Button("Remove"))
                {
                    gbCore.breakpoints[breakpointAddr] = false;
                    breakpoints.erase(std::remove(breakpoints.begin(), breakpoints.end(), static_cast<uint16_t>(breakpointAddr)), breakpoints.end());
                }
            }

            ImGui::PopStyleColor(2);

            static bool showBreakpoints{ false };

            if (ImGui::ArrowButton("##arrow", ImGuiDir_Right))
				showBreakpoints = !showBreakpoints;

            ImGui::SameLine();
            ImGui::Text("Breakpoints");

            if (showBreakpoints)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));

                if (ImGui::BeginChild("Breakpoint List", ImVec2(0, 100 * scaleFactor), true))
                {
                    for (auto it = breakpoints.begin(); it != breakpoints.end(); )
                    {
                        ImGui::PushID(*it);

                        ImGui::Text("0x%04X", *it);
                        ImGui::SameLine();

                        if (ImGui::Button("Remove"))
                        {
                            gbCore.breakpoints[*it] = false;
                            it = breakpoints.erase(it); 
                        }
                        else
                            ++it;  

                        ImGui::PopID();
                    }
                }

                ImGui::PopStyleColor(2);
                ImGui::EndChild();
            }
        }
        else
        {
            std::string romBankText = "ROM Bank (Total: " + std::to_string(gbCore.cartridge.romBanks) + ")";
            ImGui::SeparatorText(romBankText.c_str());

            if (ImGui::InputInt("##rombank", &dissasmRomBank, 1, 1, ImGuiInputTextFlags_CharsDecimal))
            {
                if (gbCore.cartridge.ROMLoaded)
                {
                    dissasmRomBank = std::clamp(dissasmRomBank, 0, static_cast<int>(gbCore.cartridge.romBanks - 1));
                    romDisassembly.resize(0);
                }
                else
                    dissasmRomBank = 0;
            }

            if (romDisassembly.empty() && gbCore.cartridge.ROMLoaded)
                disassembleRom();
        }

        ImGui::SeparatorText("Disassembly");
        if (ImGui::BeginChild("Disassembly") && gbCore.cartridge.ROMLoaded) 
        {
            ImGuiListClipper clipper;

            if (romDisassemblyView)
            {
                clipper.Begin(romDisassembly.size());

                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                        ImGui::Text("%s", romDisassembly[i].c_str());
                }
            }
            else
            {
                uint32_t instrAddr = 0;

                auto displayDisasm = [&instrAddr]()
                {
                    uint8_t instrLen;
                    std::string disasm = disassemble(static_cast<uint16_t>(instrAddr), &instrLen);

                    if (instrAddr == gbCore.cpu.s.PC) [[unlikely]]
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", disasm.c_str());
                    else if (gbCore.breakpoints[instrAddr]) [[unlikely]]
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", disasm.c_str());
                    else [[likely]]
                        ImGui::Text("%s", disasm.c_str());

                    instrAddr += instrLen;
                };

                clipper.Begin(0x10000);
                clipper.Step();

                displayDisasm();

                while (clipper.Step())
                {
                    if (clipper.DisplayStart != 1)
                        instrAddr = clipper.DisplayStart;

                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        if (instrAddr > 0xFFFF) break;
                        displayDisasm();
                    }
                }
            }
        }
        ImGui::EndChild();

        if (showBreakpointHitWindow)
        {
            ImGui::NextColumn();

            ImGui::SeparatorText("Debug");

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 0));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));

            if (ImGui::Button("Continue"))
            {
                if (tempBreakpointAddr == -1)
                {
                    gbCore.cycleCounter += gbCore.cpu.execute();
                    gbCore.breakpointHit = false;
                    showBreakpointHitWindow = false;
                }
                else
                {
                    gbCore.breakpointHit = true;
                    removeTempBreakpoint();
                    extendBreakpointDisasmWindow();
                }
            }

            ImGui::PopStyleColor(2);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));

            ImGui::SameLine();

            bool stepsDisabled = tempBreakpointAddr != -1;

            if (stepsDisabled)
                ImGui::BeginDisabled();

            if (ImGui::Button("Step Into"))
            {
                gbCore.cycleCounter += gbCore.cpu.execute();
                extendBreakpointDisasmWindow();
            }

            ImGui::SameLine();

            if (ImGui::Button("Step Over"))
            {
                if (breakpointDisassembly[breakpointDisasmLine].disasm.find("CALL") != std::string::npos)
                {
                    gbCore.breakpointHit = false;
                    gbCore.breakpoints[gbCore.cpu.s.PC + 3] = true;
                    tempBreakpointAddr = gbCore.cpu.s.PC + 3;
                }
                else
                {
					gbCore.cycleCounter += gbCore.cpu.execute();
                    extendBreakpointDisasmWindow();
				}
            }

            ImGui::Dummy(ImVec2(0, 10 * scaleFactor));

            if (ImGui::Button("Step Out"))
            {

            }

            ImGui::SameLine();

            static int stepToAddr{};

            if (ImGui::Button("Step To"))
            {
                gbCore.cycleCounter += gbCore.cpu.execute();
                gbCore.breakpoints[stepToAddr] = true;
				gbCore.breakpointHit = false;
                tempBreakpointAddr = stepToAddr;
            }

            ImGui::SameLine();

            if (ImGui::InputInt("##stepTo", &stepToAddr, 0, 0, ImGuiInputTextFlags_CharsHexadecimal))
				stepToAddr = std::clamp(stepToAddr, 0, 0xFFFF);

            if (stepsDisabled)
				ImGui::EndDisabled();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::SeparatorText("Disassembly");

            if (ImGui::BeginChild("Breakpoint Disassembly"))
            {
                if (shouldScrollToPC)
                {
                    const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY(itemHeight * breakpointDisasmLine);
                    shouldScrollToPC = false;
                }

                ImGuiListClipper clipper;
				clipper.Begin(breakpointDisassembly.size());

				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					{
                        if (i == breakpointDisasmLine)
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", breakpointDisassembly[i].disasm.c_str());
						else
							ImGui::Text("%s", breakpointDisassembly[i].disasm.c_str());
					}
				}
            }
            ImGui::EndChild();
        }

        ImGui::End();
    }
    if (showVRAMView)
    {
        static int tileViewScale { static_cast<int>(2 * scaleFactor) };
        static int tileMapViewScale = { static_cast<int>(scaleFactor) };
        static int oamViewScale = { static_cast<int>(2 * scaleFactor) };

        ImGui::Begin("VRAM View", &showVRAMView, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::BeginTabBar("tabbar"))
        {
            if (ImGui::BeginTabItem("Tile Data"))
            {
                currentTab = VRAMTab::TileData;

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

                displayImage(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileViewScale);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Background Map"))
            {
                currentTab = VRAMTab::BackgroundMap;

                if (!backgroundTexture)
                {
                    BGFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEMAP_FRAMEBUFFER_SIZE);
                    clearBuffer(BGFrameBuffer.get(), PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT);
                    OpenGL::createTexture(backgroundTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, BGFrameBuffer.get());
                }

                displayImage(backgroundTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, tileMapViewScale);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Window Map"))
            {
                currentTab = VRAMTab::WindowMap;

                if (!windowTexture)
                {
                    windowFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEMAP_FRAMEBUFFER_SIZE);
                    clearBuffer(windowFrameBuffer.get(), PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT);
                    OpenGL::createTexture(windowTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, windowFrameBuffer.get());
                }

                displayImage(windowTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, tileMapViewScale);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("OAM"))
			{
                gbCore.ppu->setOAMDebugEnable(true);
                currentTab = VRAMTab::OAM;

				if (!oamTexture)
					OpenGL::createTexture(oamTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT);

				displayImage(oamTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, oamViewScale);
				ImGui::EndTabItem();
			}
            else
                gbCore.ppu->setOAMDebugEnable(false);

            ImGui::EndTabBar();
        }
        else
            gbCore.ppu->setOAMDebugEnable(false);

        ImGui::End();
    }

    if (showAudioView)
    {
        ImGui::Begin("Audio", &showAudioView, ImGuiWindowFlags_NoResize);

        ImGui::SeparatorText("Channel Options");

        ImGui::Checkbox("Channel 1", &gbCore.apu.enableChannel1);
        ImGui::Checkbox("Channel 2", &gbCore.apu.enableChannel2);
        ImGui::Checkbox("Channel 3", &gbCore.apu.enableChannel3);
        ImGui::Checkbox("Channel 4", &gbCore.apu.enableChannel4);

        ImGui::End();
    }
}