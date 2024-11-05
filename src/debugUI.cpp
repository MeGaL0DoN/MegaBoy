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
            gbCore.setOAMDebugEnable(showVRAMView);
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
        std::string disasm = to_hex_str(addr).append(": ").append(gbCore.cpu.disassemble(addr, [](uint16_t addr) 
        {
            return gbCore.cartridge.getRom()[(dissasmRomBank * 0x4000) + (addr - (dissasmRomBank == 0 ? 0 : 0x4000))];
        }, &instrLen));

        romDisassembly.push_back(disasm);
        addr += instrLen;
    }
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
    memoryRomBank = 0;
    removeTempBreakpoint();
    showBreakpointHitWindow = false;
}

void debugUI::extendBreakpointDisasmWindow()
{
    shouldScrollToPC = true;
    uint16_t addr = gbCore.cpu.s.PC;
    uint8_t instrLen;
    std::string disasm = disassemble(addr, &instrLen);

    auto createEntry = [&addr, &instrLen, &disasm]()
    {
        std::array<uint8_t, 3> data{};

        for (int i = 0; i < instrLen; i++)
            data[i] = gbCore.mmu.read8(addr + i);

        return instructionHistoryEntry{ addr, instrLen, data, disasm };
    };
    auto isModified = [](const instructionHistoryEntry& entry) // Handle self-modifying code
    {
        for (int i = 0; i < entry.length; i++)
        {
            if (entry.data[i] != gbCore.mmu.read8(entry.addr + i))
                return true;
        }
        return false;
    };

    auto currentInstr = std::lower_bound(breakpointDisassembly.begin(), breakpointDisassembly.end(), addr);
    bool exists = currentInstr != breakpointDisassembly.end() && currentInstr->addr == addr;

    if (exists)
    {
        if (currentInstr->length != instrLen || isModified(*currentInstr))
            *currentInstr = createEntry(); 
    }
    else
        currentInstr = breakpointDisassembly.insert(currentInstr, createEntry());

    breakpointDisasmLine = currentInstr - breakpointDisassembly.begin();
    addr += instrLen;

    auto nextInstr = std::next(currentInstr);
    bool nextExists = (nextInstr != breakpointDisassembly.end() && nextInstr->addr == addr);

    if (nextExists)
    {
        if (isModified(*nextInstr))
        {
            disasm = disassemble(addr, &instrLen);
            *nextInstr = createEntry(); 
        }
    }
    else
    {
        disasm = disassemble(addr, &instrLen);
        breakpointDisassembly.insert(nextInstr, createEntry());
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
    if (stepOutStartSPVal != -1)
    {
        stepOutStartSPVal = -1;
        gbCore.cpu.setRetOpcodeEvent(nullptr);
    }
}

void debugUI::signalBreakpoint()
{
    if (tempBreakpointAddr == -1 || gbCore.cpu.s.PC != tempBreakpointAddr)
    {
        showBreakpointHitWindow = true;
        breakpointDisassembly.clear();
    }

    removeTempBreakpoint();
    extendBreakpointDisasmWindow();
}

void debugUI::updateWindows(float scaleFactor)
{
    if (showMemoryView)
    {
        ImGui::SetNextWindowSize(ImVec2(-1.f, scaleFactor * 327), ImGuiCond_Appearing);
        ImGui::Begin("Memory View", &showMemoryView);

        ImGui::RadioButton("Memory Space", &romMemoryView, 0);
        ImGui::SameLine();
        ImGui::RadioButton("ROM Banks", &romMemoryView, 1);
        ImGui::Spacing();

        if (romMemoryView)
        {
            std::string romBankText = "ROM Bank (Total: " + std::to_string(gbCore.cartridge.romBanks) + ")";
            ImGui::Text(romBankText.c_str());
            ImGui::SameLine();

            ImGui::PushItemWidth(150 * scaleFactor);

            if (ImGui::InputInt("##rombank", &memoryRomBank, 1, 1, ImGuiInputTextFlags_CharsDecimal))
            {
                if (gbCore.cartridge.ROMLoaded)
                    memoryRomBank = std::clamp(memoryRomBank, 0, static_cast<int>(gbCore.cartridge.romBanks - 1));
                else
                    memoryRomBank = 0;
            }

            ImGui::PopItemWidth();
            ImGui::Spacing();
        }

        constexpr const char* header = "Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F                    ";
        ImGui::Text(header);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("content");
        ImGuiListClipper clipper;

        auto printMem = [&clipper](uint16_t viewStartAddr, uint8_t(*readFunc)(uint16_t))
        {
            std::string memoryData(72, '\0');

            auto drawList = ImGui::GetWindowDrawList();
            const float charWidth = ImGui::CalcTextSize("0").x;
            const float vertLineX = ImGui::GetCursorScreenPos().x + (charWidth * 53.1); 

            while (clipper.Step())
            {
                const float startY = ImGui::GetCursorScreenPos().y;

                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    memoryData = "0x" + to_hex_str(static_cast<uint16_t>(viewStartAddr + i * 16)) + " ";

                    if (gbCore.cartridge.ROMLoaded)
                    {
                        std::array<uint8_t, 16> data;

                        for (int j = 0; j < 16; j++)
                        {
                            data[j] = readFunc(static_cast<uint16_t>(i * 16 + j));
                            memoryData += to_hex_str(data[j]) + " ";
                        }

                        memoryData += " ";

                        for (int j = 0; j < 16; j++)
                            memoryData += (data[j] >= 32 && data[j] <= 126) ? static_cast<char>(data[j]) : '.';
                    }
                    else
                        memoryData += "ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff  ................";

                    ImGui::Text("%s", memoryData.c_str());
                }

                const float endY = ImGui::GetCursorScreenPos().y;
                const float contentHeight = endY - startY;
                drawList->AddLine(ImVec2(vertLineX, startY), ImVec2(vertLineX, endY), ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
            }
        };

        if (romMemoryView)
        {
            clipper.Begin(1024);
            printMem(memoryRomBank == 0 ? 0 : 0x4000, [](uint16_t addr) { return gbCore.cartridge.getRom()[(memoryRomBank * 0x4000) + addr]; });
        }
        else
        {
            clipper.Begin(4096);
            printMem(0, [](uint16_t addr) { return gbCore.mmu.read8(addr); });
        }

        ImGui::EndChild();
        ImGui::End();
    }
    if (showCPUView)
    {
        ImGui::Begin("CPU View", &showCPUView, ImGuiWindowFlags_NoResize);

        ImGui::Text("PC: %04X", gbCore.cpu.s.PC);
        ImGui::Text("SP: %04X", gbCore.cpu.s.SP.val);
        ImGui::Text("IE: %02X", gbCore.cpu.s.IE);
        ImGui::SameLine();
        ImGui::Text("IF: %02X", gbCore.cpu.s.IF);
        ImGui::Text("DIV: %02X", gbCore.cpu.s.DIV_reg);
        ImGui::SameLine();
        ImGui::Text("TIMA: %02X", gbCore.cpu.s.TIMA_reg);
        ImGui::Text("Cycles: %llu", gbCore.totalCycles());
        ImGui::Text("Frequency: %.3f MHz", gbCore.cpu.s.GBCdoubleSpeed ? 2.097 : 1.048);

        ImGui::SeparatorText("Registers");

        ImGui::Text("A: %02X", gbCore.cpu.registers.A.val);
        ImGui::SameLine();
        ImGui::Text("F: %02X", gbCore.cpu.registers.F.val);
        ImGui::Text("B: %02X", gbCore.cpu.registers.B.val);
        ImGui::SameLine();
        ImGui::Text("C: %02X", gbCore.cpu.registers.C.val);
        ImGui::Text("D: %02X", gbCore.cpu.registers.D.val);
        ImGui::SameLine();
        ImGui::Text("E: %02X", gbCore.cpu.registers.E.val);
        ImGui::Text("H: %02X", gbCore.cpu.registers.H.val);
        ImGui::SameLine();
        ImGui::Text("L: %02X", gbCore.cpu.registers.L.val);

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
        ImGui::SetNextWindowSize(ImVec2(-1.f, 450 * scaleFactor), ImGuiCond_Appearing);
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

            ImGui::PushItemWidth(200 * scaleFactor);

            if (ImGui::InputInt("##addr", &breakpointAddr, 0, 0, ImGuiInputTextFlags_CharsHexadecimal))
                breakpointAddr = std::clamp(breakpointAddr, 0, 0xFFFF);

            ImGui::PopItemWidth();

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

            ImGui::PushItemWidth(200 * scaleFactor);

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

            ImGui::PopItemWidth();

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
                if (tempBreakpointAddr == -1 && stepOutStartSPVal == -1)
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

            bool stepsDisabled = tempBreakpointAddr != -1 || stepOutStartSPVal != -1;

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
                    tempBreakpointAddr = gbCore.cpu.s.PC + 3;
                    gbCore.breakpoints[tempBreakpointAddr] = true;
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
                stepOutStartSPVal = gbCore.cpu.s.SP.val;
                gbCore.breakpointHit = false;
                gbCore.cycleCounter += gbCore.cpu.execute();

                gbCore.cpu.setRetOpcodeEvent([]()
				{
                    if (gbCore.cpu.s.SP.val > stepOutStartSPVal)
                    {
                        tempBreakpointAddr = gbCore.cpu.s.PC;
                        gbCore.breakpoints[tempBreakpointAddr] = true;
                        gbCore.cpu.setRetOpcodeEvent(nullptr);
                        stepOutStartSPVal = -1;
                    }
				});
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
                    const float currentScrollY = ImGui::GetScrollY();
                    const float windowHeight = ImGui::GetWindowHeight();
                    const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
                    const float targetY = itemHeight * breakpointDisasmLine;

                    const float visibleStart = currentScrollY + itemHeight;
                    const float visibleEnd = currentScrollY + windowHeight - (2 * itemHeight);

                    if (targetY < visibleStart || targetY > visibleEnd)
                    {
                        const float centerOffset = (windowHeight - itemHeight) * 0.5f;
                        const float scrollTarget = std::max(0.0f, targetY - centerOffset);
                        ImGui::SetScrollY(scrollTarget);
                    }
                    shouldScrollToPC = false;
                }

                ImGuiListClipper clipper;
				clipper.Begin(breakpointDisassembly.size());

				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					{
                        if (breakpointDisassembly[i].addr == gbCore.cpu.s.PC)
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
                currentTab = VRAMTab::OAM;

				if (!oamTexture)
					OpenGL::createTexture(oamTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT);

				displayImage(oamTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, oamViewScale);
				ImGui::EndTabItem();
			}

            ImGui::EndTabBar();
        }

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