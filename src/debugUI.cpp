#include "debugUI.h"
#include "Utils/bitOps.h"

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
            
            if (!showVRAMView)
                gbCore.setPPUDebugEnable(false);
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
            return gbCore.cartridge.rom[(dissasmRomBank * 0x4000) + (addr - (dissasmRomBank == 0 ? 0 : 0x4000))];
        }, &instrLen));

        romDisassembly.push_back(instructionDisasmEntry { addr, instrLen, {}, disasm });
        addr += instrLen;
    }
}

void debugUI::signalVBlank() 
{
    if (!showVRAMView) return;

    auto updateTexture = [](uint32_t& texture, uint16_t width, uint16_t height, uint8_t* data)
    {
        if (!texture) 
            OpenGL::createTexture(texture, width, height, data);
        else 
            OpenGL::updateTexture(texture, width, height, data);
    };

    switch (currentTab) 
    {
    case VRAMTab::TileData:
        if (!tileDataFrameBuffer) 
            tileDataFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEDATA_FRAMEBUFFER_SIZE);

        gbCore.ppu->renderTileData(tileDataFrameBuffer.get(),System::Current() == GBSystem::DMG ? 0 : vramTileBank);
        updateTexture(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileDataFrameBuffer.get());
        break;
    case VRAMTab::BackgroundMap:
        if (!BGFrameBuffer) 
            BGFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEMAP_FRAMEBUFFER_SIZE);

        gbCore.ppu->renderBGTileMap(BGFrameBuffer.get());
        updateTexture(backgroundMapTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, BGFrameBuffer.get());
        break;
    case VRAMTab::WindowMap:
        if (!windowFrameBuffer) 
            windowFrameBuffer = std::make_unique<uint8_t[]>(PPU::TILEMAP_FRAMEBUFFER_SIZE);

        gbCore.ppu->renderWindowTileMap(windowFrameBuffer.get());
        updateTexture(windowMapTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, windowFrameBuffer.get());
        break;
    case VRAMTab::PPUOutput:
        updateTexture(oamTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->oamFramebuffer());
        updateTexture(backgroundTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->bgFramebuffer());
        updateTexture(windowTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu->windowFramebuffer());

        clearBuffer(gbCore.ppu->oamFramebuffer());
        clearBuffer(gbCore.ppu->bgFramebuffer());
        clearBuffer(gbCore.ppu->windowFramebuffer());
        break;
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

    if (texture)
    {
        OpenGL::bindTexture(texture);;
        ImGui::Image(reinterpret_cast<void*>(static_cast<uint64_t>(texture)), imageSize);
    }
    else
    {
        const auto screenPos = ImGui::GetCursorScreenPos();
        const auto color = IM_COL32(PPU::ColorPalette[0].R, PPU::ColorPalette[0].G, PPU::ColorPalette[0].B, 255);

        ImGui::GetWindowDrawList()->AddRectFilled(screenPos, ImVec2(screenPos.x + imageSize.x, screenPos.y + imageSize.y), color);
        ImGui::Dummy(imageSize);  
    }
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

        return instructionDisasmEntry{ addr, instrLen, data, disasm };
    };
    auto isModified = [](const instructionDisasmEntry& entry) // Handle self-modifying code
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

#include "CPU/regDefines.h"

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
            const std::string romBankText = "ROM Bank (Total: " + std::to_string(gbCore.cartridge.romBanks) + ")";
            ImGui::Text("%s", romBankText.c_str());
            ImGui::SameLine();

            ImGui::PushItemWidth(150 * scaleFactor);

            if (ImGui::InputInt("##rombank", &memoryRomBank, 1, 1, ImGuiInputTextFlags_CharsDecimal))
            {
                if (gbCore.cartridge.ROMLoaded())
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
            const float vertLineX = ImGui::GetCursorScreenPos().x + (charWidth * 53.1f);

            while (clipper.Step())
            {
                const float startY = ImGui::GetCursorScreenPos().y;

                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    memoryData = "0x" + to_hex_str(static_cast<uint16_t>(viewStartAddr + i * 16)) + " ";

                    if (gbCore.cartridge.ROMLoaded())
                    {
                        std::array<uint8_t, 16> data{};

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
                drawList->AddLine(ImVec2(vertLineX, startY), ImVec2(vertLineX, endY), ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
            }
        };

        if (romMemoryView)
        {
            clipper.Begin(1024);
            printMem(memoryRomBank == 0 ? 0 : 0x4000, [](uint16_t addr) { return gbCore.cartridge.rom[(memoryRomBank * 0x4000) + addr]; });
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
        ImGui::SameLine();
        ImGui::Text("| SP: %04X", gbCore.cpu.s.SP.val);
        ImGui::Text("DIV: %02X", gbCore.cpu.s.DIV_reg);
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Text("| TIMA: %02X", gbCore.cpu.s.TIMA_reg);
        ImGui::Text("Cycles: %llu", gbCore.totalCycles());
        ImGui::Text("Frequency: %.3f MHz", gbCore.cpu.s.GBCdoubleSpeed ? 2.097 : 1.048);
        ImGui::Text("Halted: %s", gbCore.cpu.s.halted ? "True" : "False");

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
        ImGui::BeginDisabled();

        bool zero = gbCore.cpu.getFlag(FlagType::Zero);
        bool carry = gbCore.cpu.getFlag(FlagType::Carry);
        bool halfCarry = gbCore.cpu.getFlag(FlagType::HalfCarry);
        bool negative = gbCore.cpu.getFlag(FlagType::Subtract);

        ImGui::Checkbox("Z", &zero);
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Checkbox("C", &carry);
        ImGui::Checkbox("H", &halfCarry);
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Checkbox("N", &negative);

        ImGui::EndDisabled();

        ImGui::SeparatorText("Interrupts");
        ImGui::BeginDisabled();

        bool ime = gbCore.cpu.s.IME;
        bool vBlank = getBit(gbCore.cpu.s.IE, 0);
        bool lcdStat = getBit(gbCore.cpu.s.IE, 1);
        bool timer = getBit(gbCore.cpu.s.IE, 2);
        bool serial = getBit(gbCore.cpu.s.IE, 3);
        bool joypad = getBit(gbCore.cpu.s.IE, 4);

        ImGui::Checkbox("Master Enable", &ime);

        ImGui::Checkbox("VBlank", &vBlank);
        ImGui::SameLine();
        ImGui::Checkbox("LCD STAT", &lcdStat);
        ImGui::Checkbox("Timer", &timer);
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Checkbox("Serial", &serial);
        ImGui::Checkbox("Joypad", &joypad);

        ImGui::EndDisabled();

        ImGui::End();
    }
    if (showDisassembly)
    {
        ImGui::SetNextWindowSize(ImVec2(280 * (static_cast<int>(showBreakpointHitWindow) + 1) * scaleFactor, 450 * scaleFactor), ImGuiCond_Appearing);
        ImGui::Begin("Disassembly", &showDisassembly);

        static bool firstTimeBreakpointWindow { true };

        if (showBreakpointHitWindow && firstTimeBreakpointWindow)
        {
            ImVec2 windowSize = ImGui::GetWindowSize();
            ImGui::SetWindowSize(ImVec2(windowSize.x * 2, windowSize.y));
            firstTimeBreakpointWindow = false;
        }

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

            if (std::find(breakpoints.begin(), breakpoints.end(), breakpointAddr) == breakpoints.end())
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
                if (ImGui::BeginChild("Breakpoint List", ImVec2(0, 100 * scaleFactor), true))
                {
                    for (auto it = breakpoints.begin(); it != breakpoints.end(); )
                    {
                        bool incrementIt = true;
                        ImGui::PushID(*it);

                        ImGui::Text("0x%04X", *it);
                        ImGui::SameLine();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));

                        if (ImGui::Button("Remove"))
                        {
                            gbCore.breakpoints[*it] = false;
                            it = breakpoints.erase(it); 
                            incrementIt = false;
                        }
                        else
                        {
                            const bool breakpointEnabled = gbCore.breakpoints[*it];
                            ImGui::SameLine();

                            if (breakpointEnabled)
                            {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.6f, 0.15f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                            }
                            else
                            {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.15f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.2f, 1.0f));
                            }

                            if (ImGui::Button(breakpointEnabled ? "Enabled" : "Disabled"))
                                gbCore.breakpoints[*it] = !breakpointEnabled;

                            ImGui::PopStyleColor(2);
                        }

                        ImGui::PopStyleColor(2);

                        if (incrementIt)
                            it++;

                        ImGui::PopID();
                    }
                }

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
                if (gbCore.cartridge.ROMLoaded())
                {
                    dissasmRomBank = std::clamp(dissasmRomBank, 0, static_cast<int>(gbCore.cartridge.romBanks - 1));
                    romDisassembly.resize(0);
                }
                else
                    dissasmRomBank = 0;
            }

            ImGui::PopItemWidth();

            if (romDisassembly.empty() && gbCore.cartridge.ROMLoaded())
                disassembleRom();
        }

        const auto displayDisasm = [](uint16_t instrAddr, const std::string& disasm)
        {
            if (instrAddr == gbCore.cpu.s.PC) [[unlikely]]
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", disasm.c_str());
            else if (gbCore.breakpoints[instrAddr]) [[unlikely]]
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", disasm.c_str());
            else [[likely]]
                ImGui::Text("%s", disasm.c_str());
        };

        ImGui::SeparatorText("Disassembly");
        if (ImGui::BeginChild("Disassembly") && gbCore.cartridge.ROMLoaded()) 
        {
            ImGuiListClipper clipper;

            if (romDisassemblyView)
            {
                clipper.Begin(static_cast<int>(romDisassembly.size()));

                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        const auto& disasm = romDisassembly[i];
                        const bool isPcInBank = dissasmRomBank == 0 || dissasmRomBank == gbCore.cartridge.getMapper()->getCurrentRomBank();

                        if (isPcInBank)
                            displayDisasm(disasm.addr, disasm.str);
                        else
                            ImGui::Text("%s", disasm.str.c_str());
                    }
                }
            }
            else
            {
                uint32_t instrAddr {0};
                uint8_t instrLen {0};

                clipper.Begin(0x10000);
                clipper.Step();

                displayDisasm(static_cast<uint16_t>(instrAddr), disassemble(instrAddr, &instrLen));
                instrAddr += instrLen;

                while (clipper.Step())
                {
                    if (clipper.DisplayStart != 1)
                        instrAddr = clipper.DisplayStart;

                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        if (instrAddr > 0xFFFF) break;
                        displayDisasm(static_cast<uint16_t>(instrAddr), disassemble(instrAddr, &instrLen));
                        instrAddr += instrLen;
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
                if (gbCore.cpu.s.halted)
                {
                    gbCore.breakpointHit = false;

                    gbCore.cpu.setHaltExitEvent([]()
                    {
                        setTempBreakpoint(gbCore.cpu.s.PC);
                        gbCore.cpu.setHaltExitEvent(nullptr);
                    });
                }
                else
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
            }

            ImGui::PopStyleColor(2);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));

            ImGui::SameLine();

            bool stepsDisabled = tempBreakpointAddr != -1 || stepOutStartSPVal != -1 || gbCore.cpu.s.halted;

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
                if (breakpointDisassembly[breakpointDisasmLine].str.find("CALL") != std::string::npos)
                {
                    gbCore.breakpointHit = false;
                    setTempBreakpoint(gbCore.cpu.s.PC + 3);
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
                        setTempBreakpoint(gbCore.cpu.s.PC);
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
                setTempBreakpoint(stepToAddr);
				gbCore.breakpointHit = false;
            }

            ImGui::SameLine();
            ImGui::PushItemWidth(95 * scaleFactor);

            if (ImGui::InputInt("##stepTo", &stepToAddr, 0, 0, ImGuiInputTextFlags_CharsHexadecimal))
				stepToAddr = std::clamp(stepToAddr, 0, 0xFFFF);

            ImGui::PopItemWidth();

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
				clipper.Begin(static_cast<int>(breakpointDisassembly.size()));

				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					{
                        if (breakpointDisassembly[i].addr == gbCore.cpu.s.PC)
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", breakpointDisassembly[i].str.c_str());
						else
							ImGui::Text("%s", breakpointDisassembly[i].str.c_str());
					}
				}
            }
            ImGui::EndChild();
        }

        ImGui::End();
    }
    if (showVRAMView)
    {
        static int tileViewScale { std::max(1,static_cast<int>(2 * scaleFactor)) };
        static int tileMapViewScale { std::max(1, static_cast<int>(scaleFactor)) };
        static int ppuOutputScale { std::max(1, static_cast<int>(2 * scaleFactor)) };

        ImGui::Begin("VRAM View", &showVRAMView, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::BeginTabBar("vramTabBar"))
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

                displayImage(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileViewScale);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tile Maps"))
            {
                if (ImGui::BeginTabBar("tileMapsTabBar"))
                {
                    if (ImGui::BeginTabItem("Background Map"))
                    {
                        currentTab = VRAMTab::BackgroundMap;
                        displayImage(backgroundMapTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, tileMapViewScale);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Window Map"))
                    {
                        currentTab = VRAMTab::WindowMap;
                        displayImage(windowMapTexture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, tileMapViewScale);
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("PPU Output"))
            {
                if (ImGui::BeginTabBar("outputTabBar"))
                {
                    currentTab = VRAMTab::PPUOutput;

                    if (ImGui::BeginTabItem("OAM"))
                    {
                        displayImage(oamTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, ppuOutputScale);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Background"))
                    {
                        displayImage(backgroundTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, ppuOutputScale);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Window"))
                    {
                        displayImage(windowTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, ppuOutputScale);
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
				ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        gbCore.setPPUDebugEnable(showVRAMView && currentTab == VRAMTab::PPUOutput);

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