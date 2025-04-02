#include "debugUI.h"
#include "Utils/bitOps.h"

#include <algorithm>
#include <ImGUI/imgui.h>

extern GBCore gb;

void debugUI::renderMenu()
{
    if (ImGui::BeginMenu("Debug"))
    {
        if (ImGui::MenuItem("Memory"))
        {
            showMemoryView = !showMemoryView;
        }
        if (ImGui::MenuItem("CPU"))
        {
            showCPUView = !showCPUView;
        }
        if (ImGui::MenuItem("Disassembly"))
        {
            showDisassembly = !showDisassembly;
			gb.enableBreakpointChecks = showDisassembly;
        }
        if (ImGui::MenuItem("PPU Registers"))
        {
            showPPUView = !showPPUView;
        }
        if (ImGui::MenuItem("VRAM"))
        {
            showVRAMView = !showVRAMView;
            
            if (!showVRAMView)
                gb.setPPUDebugEnable(false);
        }
        if (ImGui::MenuItem("Palettes"))
        {
			showPaletteView = !showPaletteView;
        }
        if (ImGui::MenuItem("Audio"))
        {
            showAudioView = !showAudioView;
        }

        ImGui::EndMenu();
    }
}

void debugUI::disassembleRom()
{
    uint8_t instrLen;
    uint16_t addr = (dissasmRomBank == 0) ? 0 : 0x4000;  
    const uint16_t endAddr = addr + 0x3FFF;

    while (addr <= endAddr)
    {
        const std::string disasm = hexOps::toHexStr<true>(addr).append(": ").append(gb.cpu.disassemble(addr, [](uint16_t addr) 
        {
            return gb.cartridge.rom[(dissasmRomBank * 0x4000) + (addr - (dissasmRomBank == 0 ? 0 : 0x4000))];
        }, &instrLen));

        romDisassembly.push_back(instructionDisasmEntry { addr, instrLen, {}, disasm });
        addr += instrLen;
    }
}

void debugUI::signalROMreset(bool cartridgeUnload)
{
    romDisassembly.clear();
    removeTempBreakpoint();

    // Don't set it to false if breakpoint is in the boot rom and boot rom will continue running (cartridge was unloaded but rom not restarted).
    if (showBreakpointHitWindow && (!cartridgeUnload || !gb.executingBootROM()))
		showBreakpointHitWindow = false;
}
void debugUI::signalSaveStateChange()
{
    removeTempBreakpoint();
    showBreakpointHitWindow = false;
}

void debugUI::refreshCurrentVRAMTab()
{
    if (!gb.executingProgram())
        return;

    const auto updateTexture = [](uint32_t& texture, uint16_t width, uint16_t height, const uint8_t* data)
    {
        if (!texture)
            OpenGL::createTexture(texture, width, height, data);
        else
            OpenGL::updateTexture(texture, width, height, data);
    };

    switch (currentVramTab)
    {
    case VRAMTab::TileData:
        if (!tileDataFramebuffer)
            tileDataFramebuffer = std::make_unique<uint8_t[]>(PPU::TILEDATA_FRAMEBUFFER_SIZE);

        gb.ppu->renderTileData(tileDataFramebuffer.get(), System::Current() == GBSystem::CGB ? vramTileBank : 0);
        updateTexture(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileDataFramebuffer.get());
        break;
    case VRAMTab::TileMap9800:
        if (!map9800Framebuffer)
            map9800Framebuffer = std::make_unique<uint8_t[]>(PPU::TILEMAP_FRAMEBUFFER_SIZE);

        gb.ppu->renderTileMap(map9800Framebuffer.get(), 0x9800);
        updateTexture(map9800Texture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, map9800Framebuffer.get());
        break;
    case VRAMTab::TileMap9C00:
        if (!map9C00Framebuffer)
            map9C00Framebuffer = std::make_unique<uint8_t[]>(PPU::TILEMAP_FRAMEBUFFER_SIZE);

        gb.ppu->renderTileMap(map9C00Framebuffer.get(), 0x9C00);
        updateTexture(map9C00Texture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, map9C00Framebuffer.get());
        break;
    case VRAMTab::PPUOutput:
        updateTexture(oamTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gb.ppu->oamFramebuffer());
        updateTexture(backgroundTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gb.ppu->bgFramebuffer());
        updateTexture(windowTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gb.ppu->windowFramebuffer());

        clearBuffer(gb.ppu->oamFramebuffer());
        clearBuffer(gb.ppu->bgFramebuffer());
        clearBuffer(gb.ppu->windowFramebuffer());
        break;
    }
}

void debugUI::signalVBlank() 
{
    if (!showVRAMView) 
        return;

    refreshCurrentVRAMTab();
}

inline void displayImage(uint32_t texture, uint16_t width, uint16_t height, int& scale)
{
    const float textWidth { ImGui::CalcTextSize("Scale: 00").x + ImGui::GetFrameHeight() * 2 + ImGui::GetStyle().ItemSpacing.x * 2 };
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);

    if (ImGui::ArrowButton("##left", ImGuiDir_Left))
        scale = std::max(1, scale - 1);

    ImGui::SameLine();
    ImGui::Text("Scale: %d", scale);
    ImGui::SameLine();

    if (ImGui::ArrowButton("##right", ImGuiDir_Right))
        scale++;

    const ImVec2 imageSize { static_cast<float>(width * scale), static_cast<float>(height * scale) };
    const float xPos { (ImGui::GetContentRegionAvail().x - imageSize.x) * 0.5f };

    if (xPos > 0)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xPos);

    if (texture && gb.executingProgram())
    {
        OpenGL::bindTexture(texture);;
        ImGui::Image((ImTextureID)(intptr_t)texture, imageSize);
    }
    else
    {
        const auto screenPos { ImGui::GetCursorScreenPos() };
        const auto color { IM_COL32(PPU::ColorPalette[0].R, PPU::ColorPalette[0].G, PPU::ColorPalette[0].B, 255) };

        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetCursorScreenPos(), ImVec2(screenPos.x + imageSize.x, screenPos.y + imageSize.y), color);
        ImGui::Dummy(imageSize);  
    }
}

inline std::string disassemble(uint16_t addr, uint8_t* instrLen)
{
    return hexOps::toHexStr<true>(addr).append(": ").append(gb.cpu.disassemble(addr, [](uint16_t addr) { return gb.mmu.read8(addr); }, instrLen));
}

void debugUI::extendBreakpointDisasmWindow()
{
    shouldScrollToPC = true;
    uint16_t addr { gb.cpu.s.PC };
    uint8_t instrLen;
    std::string disasm { disassemble(addr, &instrLen) };

    const auto createEntry = [&]()
    {
        std::array<uint8_t, 3> data{};

        for (int i = 0; i < instrLen; i++)
            data[i] = gb.mmu.read8(addr + i);

        return instructionDisasmEntry{ addr, instrLen, data, disasm };
    };
    const auto isModified = [](const instructionDisasmEntry& entry) 
    {
        for (int i = 0; i < entry.length; i++)
        {
            if (entry.data[i] != gb.mmu.read8(entry.addr + i))
                return true;
        }
        return false;
    };

    auto currentInstr { std::lower_bound(breakpointDisassembly.begin(), breakpointDisassembly.end(), addr) };
    const bool exists { currentInstr != breakpointDisassembly.end() && currentInstr->addr == addr };

    if (exists)
    {
        if (currentInstr->length != instrLen || isModified(*currentInstr))
            *currentInstr = createEntry(); 
    }
    else
        currentInstr = breakpointDisassembly.insert(currentInstr, createEntry());

    breakpointDisasmLine = currentInstr - breakpointDisassembly.begin();
    addr += instrLen;

    const auto nextInstr { std::next(currentInstr) };
    const bool nextExists { (nextInstr != breakpointDisassembly.end() && nextInstr->addr == addr) };

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
        if (std::ranges::find(breakpoints, tempBreakpointAddr) == breakpoints.end())
            gb.breakpoints[tempBreakpointAddr] = false;

        tempBreakpointAddr = -1;
    }
    if (stepOutStartSPVal != -1)
    {
        stepOutStartSPVal = -1;
        gb.cpu.setRetOpcodeEvent(nullptr);
    }
}

void debugUI::signalBreakpoint()
{
    if (tempBreakpointAddr == -1 || gb.cpu.s.PC != tempBreakpointAddr)
    {
        showBreakpointHitWindow = true;
        breakpointDisassembly.clear();
    }

    removeTempBreakpoint();
    extendBreakpointDisasmWindow();
}


#include <ImGUI/imgui_internal.h>

void debugUI::renderWindows(float scaleFactor)
{
    if (showMemoryView)
    {
        ImGui::SetNextWindowSize(ImVec2(-1.f, scaleFactor * 350), ImGuiCond_Appearing);

        if (ImGui::Begin("Memory View", &showMemoryView))
        {
            static MemView currentView { MemView::MemSpace };
            static int memViewRomBank { 0 }, memViewRamBank { 0 }, memViewWramBank { 0 }, memViewVramBank { 0 };

            ImGui::RadioButton("Mem Space", reinterpret_cast<int*>(&currentView), static_cast<int>(MemView::MemSpace));
            ImGui::SameLine();
            ImGui::RadioButton("ROM", reinterpret_cast<int*>(&currentView), static_cast<int>(MemView::ROM));
            ImGui::SameLine();
            ImGui::RadioButton("WRAM", reinterpret_cast<int*>(&currentView), static_cast<int>(MemView::WRAM));
            ImGui::SameLine();

            if (gb.cartridge.hasRAM)
            {
                ImGui::RadioButton("SRAM", reinterpret_cast<int*>(&currentView), static_cast<int>(MemView::SRAM));
                ImGui::SameLine();
            }
            else if (currentView == MemView::SRAM)
                currentView = MemView::WRAM;

            ImGui::RadioButton("VRAM", reinterpret_cast<int*>(&currentView), static_cast<int>(MemView::VRAM));
            ImGui::SameLine();
            ImGui::RadioButton("OAM", reinterpret_cast<int*>(&currentView), static_cast<int>(MemView::OAM));
            ImGui::SameLine();
            ImGui::RadioButton("IO", reinterpret_cast<int*>(&currentView), static_cast<int>(MemView::IO));
            ImGui::SameLine();
            ImGui::RadioButton("HRAM", reinterpret_cast<int*>(&currentView), static_cast<int>(MemView::HRAM));
            ImGui::Spacing();

            switch (currentView)
            {
            case MemView::ROM:
            {
                const std::string romBankText{ "ROM Bank (Total: " + std::to_string(gb.cartridge.romBanks) + ")" };
                ImGui::Text("%s", romBankText.c_str());

                ImGui::SameLine();
                ImGui::PushItemWidth(150 * scaleFactor);

                ImGui::InputInt("##rombank", &memViewRomBank, 1, 1, ImGuiInputTextFlags_CharsDecimal);
                memViewRomBank = std::clamp(memViewRomBank, 0, static_cast<int>(gb.cartridge.romBanks - 1));

                ImGui::PopItemWidth();
                ImGui::Spacing();
                break;
            }
            case MemView::WRAM:
            {
                if (System::Current() == GBSystem::CGB)
                {
                    ImGui::Text("RAM Bank (Total: 8)");

                    ImGui::SameLine();
                    ImGui::PushItemWidth(150 * scaleFactor);

                    if (ImGui::InputInt("##rambank", &memViewWramBank, 1, 1, ImGuiInputTextFlags_CharsDecimal))
                        memViewWramBank = std::clamp(memViewWramBank, 0, 7);

                    ImGui::PopItemWidth();
                }
                else
                {
                    memViewWramBank = std::clamp(memViewWramBank, 0, 1);

                    ImGui::RadioButton("Bank 0", &memViewWramBank, 0);
                    ImGui::SameLine();
                    ImGui::RadioButton("Bank 1", &memViewWramBank, 1);
                }

                ImGui::Spacing();
                break;
            }
            case MemView::SRAM:
            {
                const std::string ramBankText { "SRAM Bank (Total: " + std::to_string(gb.cartridge.ramBanks) + ")" };
                ImGui::Text("%s", ramBankText.c_str());

                ImGui::SameLine();
                ImGui::PushItemWidth(150 * scaleFactor);

                ImGui::InputInt("##rambank", &memViewRamBank, 1, 1, ImGuiInputTextFlags_CharsDecimal);
                memViewRamBank = std::clamp(memViewRamBank, 0, static_cast<int>(gb.cartridge.ramBanks - 1));

                ImGui::PopItemWidth();
                ImGui::Spacing();
                break;
            }
            case MemView::VRAM:
            {
                if (System::Current() == GBSystem::CGB)
                {
                    ImGui::RadioButton("Bank 0", &memViewVramBank, 0);
                    ImGui::SameLine();
                    ImGui::RadioButton("Bank 1", &memViewVramBank, 1);
                    ImGui::Spacing();
                }
                break;
            }
            default:
                break;
            }

            constexpr const char* header = "Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F                    ";
            ImGui::Text(header);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::BeginChild("content"))
            {
                ImGuiListClipper clipper;

                const auto printMem = [&clipper](uint16_t viewStartAddr, uint8_t(*readFunc)(uint16_t))
                {
                    std::string memoryData(72, '\0');

                    const float charWidth { ImGui::CalcTextSize("0").x };
                    const float vertLineX { ImGui::GetCursorScreenPos().x + (charWidth * 53.1f) };

                    while (clipper.Step())
                    {
                        const float startY { ImGui::GetCursorScreenPos().y };

                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                        {
                            memoryData = "0x" + hexOps::toHexStr<true>(static_cast<uint16_t>(viewStartAddr + i * 16)) + " ";

                            if (gb.executingProgram())
                            {
                                std::array<uint8_t, 16> data{};

                                for (int j = 0; j < 16; j++)
                                {
                                    data[j] = readFunc(static_cast<uint16_t>(i * 16 + j));
                                    memoryData += hexOps::toHexStr<false>(data[j]) + " ";
                                }

                                memoryData += " ";

                                for (int j = 0; j < 16; j++)
                                    memoryData += (data[j] >= 32 && data[j] <= 126) ? static_cast<char>(data[j]) : '.';
                            }
                            else
                                memoryData += "ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff  ................";

                            ImGui::Text("%s", memoryData.c_str());
                        }

                        const float endY { ImGui::GetCursorScreenPos().y };
                        ImGui::GetWindowDrawList()->AddLine(ImVec2(vertLineX, startY), ImVec2(vertLineX, endY), ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
                    }
                };

                switch (currentView)
                {
                case MemView::MemSpace:
                    clipper.Begin(0x10000 / 16);
                    printMem(0, [](uint16_t addr) { return gb.mmu.read8(addr); });
                    break;
                case MemView::ROM:
                    clipper.Begin(0x4000 / 16);
                    printMem(memViewRomBank == 0 ? 0 : 0x4000, [](uint16_t addr) { return gb.cartridge.rom[(memViewRomBank * 0x4000) + addr]; });
                    break;
                case MemView::WRAM:
                    clipper.Begin(0x1000 / 16);
                    printMem(memViewWramBank == 0 ? 0xC000 : 0xD000, [](uint16_t addr) { return gb.mmu.WRAM_BANKS[(memViewWramBank * 0x1000) + addr]; });
                    break;
                case MemView::SRAM:
                    clipper.Begin(0x2000 / 16);
                    printMem(0xA000, [](uint16_t addr) { return gb.cartridge.ram[(memViewRamBank) * 0x2000 + addr]; });
                    break;
                case MemView::VRAM:
                    clipper.Begin(0x2000 / 16);
                    printMem(0x8000, [](uint16_t addr) 
                    {
                        return System::Current() != GBSystem::CGB || memViewVramBank == 0 ? gb.ppu->VRAM_BANK0[addr] : gb.ppu->VRAM_BANK1[addr];
                    });
                    break;
                case MemView::OAM:
                    clipper.Begin(sizeof(PPU::OAM) / 16);
                    printMem(0xFE00, [](uint16_t addr) { return gb.ppu->OAM[addr]; });
                    break;
                case MemView::IO:
                    clipper.Begin(0x80 / 16);
                    printMem(0xFF00, [](uint16_t addr) { return gb.mmu.read8(addr + 0xFF00); });
                    break;
                case MemView::HRAM:
                    clipper.Begin((sizeof(MMU::HRAM) / 16) + 1);
                    printMem(0xFF80, [](uint16_t addr) { return addr == sizeof(MMU::HRAM) ? gb.cpu.s.IE : gb.mmu.HRAM[addr]; });
                    break;
                }
            }

            ImGui::EndChild();
        }

        ImGui::End();
    }
    if (showCPUView)
    {
        if (ImGui::Begin("CPU View", &showCPUView, ImGuiWindowFlags_NoResize))
        {
            ImGui::TextColored(ImVec4(255, 255, 0, 255), "PC: $%04X", gb.cpu.s.PC);
            ImGui::SameLine();
            ImGui::Text("| SP: $%04X", gb.cpu.s.SP.val);
            ImGui::Text("DIV: $%02X", gb.cpu.s.divCounter >> 8);
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::Text("| TIMA: $%02X", gb.cpu.s.timaReg);
            ImGui::Text("TAC: $%02X", gb.cpu.s.tacReg);
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::Text("| TMA: $%02X", gb.cpu.s.tmaReg);
            ImGui::Text("Halted: %s", gb.cpu.s.halted ? "True" : "False");
            ImGui::Text("Cycles: %llu", (gb.cycleCount() / gb.cpu.TcyclesPerM())); // Displaying M cycles
            ImGui::Text("Frequency: %.3f MHz", gb.cpu.doubleSpeedMode() ? 2.097 : 1.048);
            ImGui::TextColored(ImVec4(255, 255, 0, 255), "CPU Usage: %.2f%%", gb.getCPUUsage());

            ImGui::SeparatorText("Registers");

            ImGui::Text("A: $%02X", gb.cpu.registers.AF.high.val);
            ImGui::SameLine();
            ImGui::Text("F: $%02X", gb.cpu.registers.AF.low.val);
            ImGui::Text("B: $%02X", gb.cpu.registers.BC.high.val);
            ImGui::SameLine();
            ImGui::Text("C: $%02X", gb.cpu.registers.BC.low.val);
            ImGui::Text("D: $%02X", gb.cpu.registers.DE.high.val);
            ImGui::SameLine();
            ImGui::Text("E: $%02X", gb.cpu.registers.DE.low.val);
            ImGui::Text("H: $%02X", gb.cpu.registers.HL.high.val);
            ImGui::SameLine();
            ImGui::Text("L: $%02X", gb.cpu.registers.HL.low.val);

            ImGui::SeparatorText("Flags");
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

            bool zero { gb.cpu.getFlag(FlagType::Zero) };
            bool carry { gb.cpu.getFlag(FlagType::Carry) };
            bool halfCarry { gb.cpu.getFlag(FlagType::HalfCarry) };
            bool negative { gb.cpu.getFlag(FlagType::Subtract) };

            ImGui::Checkbox("Z", &zero);
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();

            ImGui::Checkbox("C", &carry);
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();

            ImGui::Checkbox("H", &halfCarry);
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();

            ImGui::Checkbox("N", &negative);

            ImGui::SeparatorText("Interrupts");

            bool ime = gb.cpu.s.IME;
            bool vBlank = getBit(gb.cpu.s.IE, 0);
            bool lcdStat = getBit(gb.cpu.s.IE, 1);
            bool timer = getBit(gb.cpu.s.IE, 2);
            bool serial = getBit(gb.cpu.s.IE, 3);
            bool joypad = getBit(gb.cpu.s.IE, 4);

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

            ImGui::PopItemFlag();
        }

        ImGui::End();
    }
    if (showDisassembly)
    {
        ImGui::SetNextWindowSize(ImVec2(280.0f * (static_cast<float>(showBreakpointHitWindow) + 1) * scaleFactor, 450 * scaleFactor), ImGuiCond_Appearing);

        if (ImGui::Begin("Disassembly", &showDisassembly))
        {
            static bool firstTimeBreakpointWindow { true };

            if (showBreakpointHitWindow && firstTimeBreakpointWindow)
            {
                const auto windowSize { ImGui::GetWindowSize() };
                ImGui::SetWindowSize(ImVec2(windowSize.x * 2, windowSize.y));
                firstTimeBreakpointWindow = false;
            }

            if (showBreakpointHitWindow)
                ImGui::Columns(2);

            ImGui::SeparatorText("Disassembly View");
            static int romDisassemblyView { false };

            ImGui::RadioButton("Memory Space", &romDisassemblyView, false);
            ImGui::SameLine();
            ImGui::RadioButton("ROM Banks", &romDisassemblyView, true);

            if (!romDisassemblyView)
            {
                static int breakpointAddr{};
                static int breakpointOpcode{};

                const auto renderBreakpointControl = [&](const char* label, int& value, auto& breakpointArr, auto& breakpointList, int maxValue)
                {
                    ImGui::PushID(label);
                    ImGui::SeparatorText(label);

                    ImGui::PushItemWidth(200 * scaleFactor);

                    if (ImGui::InputInt("##input", &value, 0, 0, ImGuiInputTextFlags_CharsHexadecimal))
                        value = std::clamp(value, 0, maxValue);

                    ImGui::PopItemWidth();

                    ImGui::SameLine();
                    const bool exists { std::ranges::find(breakpointList, value) != breakpointList.end() };

                    ImGui::PushStyleColor(ImGuiCol_Button, exists ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, exists ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 0.5f, 0.9f, 1.0f));

                    const bool textBoxEnterWasPressed { ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter) };
                    const char* buttonLabel { exists ? "Remove" : "Add" };

                    if (ImGui::Button(buttonLabel) || (!exists && textBoxEnterWasPressed))
                    {
                        breakpointArr[value] = !exists;
                        if (exists)
                        {
                            breakpointList.erase (
                                std::remove(breakpointList.begin(), breakpointList.end(), value),
                                breakpointList.end()
                            );
                        }
                        else
                            breakpointList.push_back(value);
                    };

                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                };

                const auto renderBreakpointItems = [](auto& breakpointArr, auto& breakpointList, const char* formatStr)
                {
                    for (auto it = breakpointList.begin(); it != breakpointList.end(); )
                    {
                        bool incrementIt = true;
                        ImGui::PushID(*it);

                        ImGui::Text(formatStr, *it);
                        ImGui::SameLine();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));

                        if (ImGui::Button("Remove"))
                        {
                            breakpointArr[*it] = false;
                            it = breakpointList.erase(it);
                            incrementIt = false;
                        }
                        else
                        {
                            const bool breakpointEnabled { breakpointArr[*it] };
                            ImGui::SameLine();

                            const auto [btnColor, btnHoverColor] = breakpointEnabled ?
                                std::make_pair(ImVec4(0.15f, 0.6f, 0.15f, 1.0f), ImVec4(0.2f, 0.7f, 0.2f, 1.0f)) :
                                std::make_pair(ImVec4(0.6f, 0.6f, 0.15f, 1.0f), ImVec4(0.7f, 0.7f, 0.2f, 1.0f));

                            ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHoverColor);

                            if (ImGui::Button(breakpointEnabled ? "Enabled" : "Disabled"))
                                breakpointArr[*it] = !breakpointEnabled;

                            ImGui::PopStyleColor(2);
                        }

                        ImGui::PopStyleColor(2);

                        if (incrementIt)
                            ++it;

                        ImGui::PopID();
                    }
                };

                renderBreakpointControl("Breakpoint on Address", breakpointAddr, gb.breakpoints, breakpoints, 0xFFFF);
                renderBreakpointControl("Breakpoint on Opcode", breakpointOpcode, gb.opcodeBreakpoints, opcodeBreakpoints, 0xFF);

                static bool showBreakpoints { false };

                ImGui::Spacing();

                if (ImGui::ArrowButton("##arrow", ImGuiDir_Right))
                    showBreakpoints = !showBreakpoints;

                ImGui::SameLine();
                ImGui::Text("Breakpoints");

                if (showBreakpoints)
                {
                    if (ImGui::BeginChild("Breakpoint List", ImVec2(0, 150), ImGuiChildFlags_Borders))
                    {
                        ImGui::PushID(0);
                        renderBreakpointItems(gb.breakpoints, breakpoints, "%04X");
                        ImGui::PopID();

                        if (!breakpoints.empty() && !opcodeBreakpoints.empty())
                        {
                            ImGui::Spacing();
                            ImGui::Separator();
                            ImGui::Spacing();
                        }

                        renderBreakpointItems(gb.opcodeBreakpoints, opcodeBreakpoints, "0x%02X");
                    }
                    ImGui::EndChild();
                }
            }
            else
            {
                const std::string romBankText { "ROM Bank (Total: " + std::to_string(gb.cartridge.romBanks) + ")" };
                ImGui::SeparatorText(romBankText.c_str());

                ImGui::PushItemWidth(200 * scaleFactor);

                if (ImGui::InputInt("##rombank", &dissasmRomBank, 1, 1, ImGuiInputTextFlags_CharsDecimal))
                    romDisassembly.clear();

                dissasmRomBank = std::clamp(dissasmRomBank, 0, static_cast<int>(gb.cartridge.romBanks - 1));

                ImGui::PopItemWidth();

                if (romDisassembly.empty() && gb.executingProgram())
                    disassembleRom();
            }

            const auto displayDisasm = [](uint16_t instrAddr, const std::string& disasm)
            {
                if (instrAddr == gb.cpu.s.PC) [[unlikely]]
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", disasm.c_str());
                else if (gb.breakpoints[instrAddr]) [[unlikely]]
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", disasm.c_str());
                else [[likely]]
                    ImGui::Text("%s", disasm.c_str());
            };

            ImGui::SeparatorText("Disassembly");
            if (ImGui::BeginChild("Disassembly") && gb.executingProgram())
            {
                ImGuiListClipper clipper;

                if (romDisassemblyView)
                {
                    clipper.Begin(static_cast<int>(romDisassembly.size()));

                    while (clipper.Step())
                    {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                        {
                            const auto& disasm { romDisassembly[i] };
                            const bool isPcInBank { dissasmRomBank == 0 || dissasmRomBank == gb.cartridge.getMapper()->getCurrentRomBank() };

                            if (isPcInBank)
                                displayDisasm(disasm.addr, disasm.str);
                            else
                                ImGui::Text("%s", disasm.str.c_str());
                        }
                    }
                }
                else
                {
                    uint32_t instrAddr{ 0 };
                    uint8_t instrLen{ 0 };

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
                const static auto setTempBreakpoint = [](uint16_t addr)
                {
                    tempBreakpointAddr = addr;
                    gb.breakpoints[addr] = true;
                };

                ImGui::NextColumn();
                ImGui::SeparatorText("Debug");

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 0));

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.6f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));

                if (ImGui::Button("Continue"))
                {
                    if (gb.cpu.s.halted)
                    {
                        static uint16_t haltPC{};

                        gb.breakpointHit = false;
                        haltPC = gb.cpu.s.PC;
                        gb.breakpoints[haltPC] = false;

                        gb.cpu.setHaltExitEvent([]()
                        {
                            if (std::ranges::find(breakpoints, haltPC) != breakpoints.end())
                                gb.breakpoints[haltPC] = true;

                            gb.breakpointHit = true;
                            gb.cpu.setHaltExitEvent(nullptr);
                            extendBreakpointDisasmWindow();
                        });
                    }
                    else
                    {
                        if (tempBreakpointAddr == -1 && stepOutStartSPVal == -1)
                        {
                            gb.cycleCounter += gb.cpu.execute();
                            gb.breakpointHit = false;
                            showBreakpointHitWindow = false;
                        }
                        else
                        {
                            gb.breakpointHit = true;
                            removeTempBreakpoint();
                            extendBreakpointDisasmWindow();
                        }
                    }
                }

                if (gb.cpu.s.halted)
                {
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("CPU Halted!");
                }

                ImGui::PopStyleColor(2);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));

                ImGui::SameLine();

                const bool stepsDisabled { tempBreakpointAddr != -1 || stepOutStartSPVal != -1 || gb.cpu.s.halted };

                if (stepsDisabled)
                    ImGui::BeginDisabled();

                if (ImGui::Button("Step Into"))
                {
                    gb.cycleCounter += gb.cpu.execute();
                    extendBreakpointDisasmWindow();
                }

                ImGui::SameLine();

                if (ImGui::Button("Step Over"))
                {
                    if (breakpointDisassembly[breakpointDisasmLine].str.find("CALL") != std::string::npos)
                    {
                        gb.breakpointHit = false;
                        setTempBreakpoint(gb.cpu.s.PC + 3);
                        gb.cycleCounter += gb.cpu.execute();
                    }
                    else
                    {
                        gb.cycleCounter += gb.cpu.execute();
                        extendBreakpointDisasmWindow();
                    }
                }

                ImGui::Dummy(ImVec2(0, 10 * scaleFactor));

                if (ImGui::Button("Step Out"))
                {
                    stepOutStartSPVal = gb.cpu.s.SP.val;
                    gb.breakpointHit = false;
                    gb.cycleCounter += gb.cpu.execute();

                    gb.cpu.setRetOpcodeEvent([]()
                    {
                        if (gb.cpu.s.SP.val > stepOutStartSPVal)
                        {
                            setTempBreakpoint(gb.cpu.s.PC);
                            gb.cpu.setRetOpcodeEvent(nullptr);
                            stepOutStartSPVal = -1;
                        }
                    });
                }

                ImGui::SameLine();

                static int stepToAddr{};

                if (ImGui::Button("Step To"))
                {
                    gb.cycleCounter += gb.cpu.execute();
                    setTempBreakpoint(stepToAddr);
                    gb.breakpointHit = false;
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
                        const float currentScrollY { ImGui::GetScrollY() };
                        const float windowHeight { ImGui::GetWindowHeight() };
                        const float itemHeight { ImGui::GetTextLineHeightWithSpacing() };
                        const float targetY { itemHeight * static_cast<float>(breakpointDisasmLine) };

                        const float visibleStart { currentScrollY + itemHeight };
                        const float visibleEnd { currentScrollY + windowHeight - (2 * itemHeight) };

                        if (targetY < visibleStart || targetY > visibleEnd)
                        {
                            const float centerOffset { (windowHeight - itemHeight) * 0.5f };
                            const float scrollTarget { std::max(0.0f, targetY - centerOffset) };
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
                            if (breakpointDisassembly[i].addr == gb.cpu.s.PC)
                                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", breakpointDisassembly[i].str.c_str());
                            else
                                ImGui::Text("%s", breakpointDisassembly[i].str.c_str());
                        }
                    }
                }
                ImGui::EndChild();
            }
        }

        ImGui::End();

        if (!showDisassembly)
			gb.enableBreakpointChecks = false;
    }
    if (showPPUView)
    {
        if (ImGui::Begin("PPU View", &showPPUView, ImGuiWindowFlags_NoResize))
        {
            static std::string lcdcText, statText;

            lcdcText = "LCDC: $" + hexOps::toHexStr<true>(gb.ppu->regs.LCDC);
            ImGui::SeparatorText(lcdcText.c_str());

            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

            bool lcdEnable = getBit(gb.ppu->regs.LCDC, 7) ;
            bool windowTileMap = getBit(gb.ppu->regs.LCDC, 6);
            bool windowEnable = getBit(gb.ppu->regs.LCDC, 5);
            bool tileDataArea = getBit(gb.ppu->regs.LCDC, 4);
            bool bgTileMap = getBit(gb.ppu->regs.LCDC, 3);
            bool objSize = getBit(gb.ppu->regs.LCDC, 2);
            bool objEnable = getBit(gb.ppu->regs.LCDC, 1);
            bool lcdcBit0 = getBit(gb.ppu->regs.LCDC, 0);

            ImGui::Checkbox("LCD Enable", &lcdEnable);
            ImGui::Checkbox("$9C00 Window Map", &windowTileMap);
            ImGui::Checkbox("$9C00 BG Map", &bgTileMap);
            ImGui::Checkbox("$8000 Tile Addressing", &tileDataArea);
            ImGui::Checkbox("Window Enable", &windowEnable);

            ImGui::Checkbox("8x16 OBJ Size", &objSize);
            ImGui::SameLine();
            ImGui::Checkbox("OBJ Enable", &objEnable);

            if (System::Current() == GBSystem::CGB)          
                ImGui::Checkbox("BG and Window Priority", &lcdcBit0);
            else 
                ImGui::Checkbox("BG and Window Enable", &lcdcBit0);

            statText = "STAT: $" + hexOps::toHexStr<true>(gb.ppu->readSTAT());
            ImGui::SeparatorText(statText.c_str());

            bool lycStat = getBit(gb.ppu->regs.STAT, 6);
            bool oamScanStat = getBit(gb.ppu->regs.STAT, 5);
            bool vblankStat = getBit(gb.ppu->regs.STAT, 4);
            bool hblankStat =  getBit(gb.ppu->regs.STAT, 3);
            bool lycFlag = getBit(gb.ppu->regs.STAT, 2);

            ImGui::Checkbox("LYC-LY STAT", &lycStat);
            ImGui::SameLine();
            ImGui::Checkbox("LYC Flag", &lycFlag);

            ImGui::Checkbox("VBlank STAT", &vblankStat);
            ImGui::SameLine();
            ImGui::Checkbox("HBlank STAT", &hblankStat);

            ImGui::Checkbox("OAM Scan STAT", &oamScanStat);

            ImGui::PopItemFlag();
            ImGui::SeparatorText("Misc.");

            ImGui::Text("LY: %d", gb.ppu->s.LY);
            ImGui::Text("LYC: %d", gb.ppu->regs.LYC);

            ImGui::Spacing();

            ImGui::Text("PPU Mode: %s", gb.ppu->s.state == PPUMode::HBlank ? "HBlank" : gb.ppu->s.state == PPUMode::VBlank ? "VBlank" :
                                        gb.ppu->s.state == PPUMode::PixelTransfer ? "Pixel Transfer" : "OAM Scan");

            ImGui::Text("Mode Dot Counter: %d", gb.ppu->s.videoCycles);

            if (lcdEnable)
                ImGui::TextColored(ImVec4(255, 255, 0, 255), "Dots Until VBlank: %d", gb.ppu->s.dotsUntilVBlank);
            else
                ImGui::TextColored(ImVec4(255, 255, 0, 255), "Dots Until VBlank: ? (LCD off)");
        }

        ImGui::End();
    }
    if (showVRAMView)
    {
        static int tileViewScale { std::max(1,static_cast<int>(2 * scaleFactor)) };
        static int tileMapViewScale { std::max(1, static_cast<int>(scaleFactor)) };
        static int ppuOutputScale { std::max(1, static_cast<int>(2 * scaleFactor)) };

        if (ImGui::Begin("VRAM View", &showVRAMView, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::BeginTabBar("vramTabBar"))
            {
                const auto displayRefreshButton = []()
                {
                    ImGui::Spacing();

                    const float buttonWidth { ImGui::CalcTextSize("Refresh").x + ImGui::GetStyle().FramePadding.x * 2 };
                    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - buttonWidth) * 0.5f);

                    if (ImGui::Button("Refresh"))
                        refreshCurrentVRAMTab();

                    ImGui::Spacing();
                };

                if (ImGui::BeginTabItem("Tile Data"))
                {
                    currentVramTab = VRAMTab::TileData;

                    if (System::Current() == GBSystem::CGB)
                    {
                        ImGui::RadioButton("VRAM Bank 0", &vramTileBank, 0);
                        ImGui::SameLine();
                        ImGui::RadioButton("VRAM Bank 1", &vramTileBank, 1);
                    }

                    displayRefreshButton();
                    displayImage(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileViewScale);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Tile Maps"))
                {
                    if (ImGui::BeginTabBar("tileMapsTabBar"))
                    {
                        if (ImGui::BeginTabItem("9800 Map"))
                        {
                            currentVramTab = VRAMTab::TileMap9800;
                            displayRefreshButton();
                            displayImage(map9800Texture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, tileMapViewScale);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("9C00 Map"))
                        {
                            currentVramTab = VRAMTab::TileMap9C00;
                            displayRefreshButton();
                            displayImage(map9C00Texture, PPU::TILEMAP_WIDTH, PPU::TILEMAP_HEIGHT, tileMapViewScale);
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
                        currentVramTab = VRAMTab::PPUOutput;

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
        }

        gb.setPPUDebugEnable(showVRAMView && currentVramTab == VRAMTab::PPUOutput);
        ImGui::End();
    }
    if (showPaletteView)
    {
        if (ImGui::Begin("Palettes", &showPaletteView, ImGuiWindowFlags_AlwaysAutoResize))
        {
            constexpr int COLORS = 4;

            const auto drawList { ImGui::GetWindowDrawList() };
            const int squareSize = 35 * scaleFactor, gapSize = 5 * scaleFactor;

            const auto gbcColorToRGB5 = [](int palette, int color, const std::array<uint8_t, 64>& paletteRam) -> uint16_t
            {
                const int colorInd { palette * 8 + color * 2 };
                const uint8_t low { paletteRam[colorInd] }, high { paletteRam[colorInd + 1] };
                return high << 8 | low;
            };

            if (System::Current() == GBSystem::CGB && gb.executingProgram())
            {
                constexpr int PALETTES = 8;

                const int columnWidth { COLORS * (squareSize + gapSize) };
                const auto pos { ImGui::GetCursorScreenPos() };

                ImGui::SetCursorScreenPos(ImVec2(pos.x + columnWidth / 2 - ImGui::CalcTextSize("BCPS").x / 2, pos.y));
                ImGui::Text("BCPS");

                ImGui::SetCursorScreenPos(ImVec2(pos.x + columnWidth + gapSize * 3 + columnWidth / 2 - ImGui::CalcTextSize("OCPS").x / 2, pos.y));
                ImGui::Text("OCPS");

                ImGui::Separator();
                ImGui::Spacing();

                const auto startPos { ImGui::GetCursorScreenPos() };

                const auto drawPaletteRAM = [&](int xOffset, const std::array<uint8_t, 64>& paletteRam)
                {
                    for (int y = 0; y < PALETTES; y++)
                    {
                        for (int x = 0; x < COLORS; x++)
                        {
                            const uint16_t rgb5 { gbcColorToRGB5(y, x, paletteRam) };
                            const auto col { color::fromRGB5(rgb5, appConfig::gbcColorCorrection) };

                            const auto pos { ImVec2(startPos.x + xOffset + x * (squareSize + gapSize), startPos.y + y * (squareSize + gapSize)) };
                            drawList->AddRectFilled(pos, ImVec2(pos.x + squareSize, pos.y + squareSize), IM_COL32(col.R, col.G, col.B, 255));

                            ImGui::SetCursorScreenPos(pos);
                            ImGui::Dummy(ImVec2(squareSize, squareSize));

                            std::stringstream ss;

                            ss << "ID: " << x << " | Palette: " << y << '\n';
                            ss << "Value: $" << hexOps::toHexStr<true>(rgb5) << '\n';
                            ss << "RGB5: " << (rgb5 & 0x1F) << " " << ((rgb5 >> 5) & 0x1F) << " " << ((rgb5 >> 10) & 0x1F);

                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("%s", ss.str().c_str());
                        }
                    }
                };

                drawPaletteRAM(0, gb.ppu->gbcRegs.BCPS.RAM);
                drawPaletteRAM(columnWidth + gapSize * 3, gb.ppu->gbcRegs.OCPS.RAM);
            }
            else
            {
                enum class DMGPalette { BGP, OBP0, OBP1 };

                const auto drawPalette = [&](DMGPalette p)
                {
                    const auto startPos { ImGui::GetCursorScreenPos() };

                    for (int i = 0; i < COLORS; i++)
                    {
                        const auto palette { p == DMGPalette::BGP ? gb.ppu->BGP : p == DMGPalette::OBP0 ? gb.ppu->OBP0 : gb.ppu->OBP1 };
                        const int colInd { palette[i] };

                        color col;
					    uint16_t rgb5;

                        if (System::Current() == GBSystem::DMGCompatMode)
                        {
                            const auto& ram { p == DMGPalette::BGP ? gb.ppu->gbcRegs.BCPS.RAM : gb.ppu->gbcRegs.OCPS.RAM };
                            const int palette { p == DMGPalette::OBP1 ? 1 : 0 };

							rgb5 = gbcColorToRGB5(palette, colInd, ram);
							col = color::fromRGB5(rgb5, appConfig::gbcColorCorrection);
                        }
                        else
                            col = PPU::ColorPalette[colInd];

						const auto pos { ImVec2(startPos.x + i * (squareSize + gapSize), startPos.y) };
						drawList->AddRectFilled(pos, ImVec2(pos.x + squareSize, pos.y + squareSize), IM_COL32(col.R, col.G, col.B, 255));

						ImGui::SetCursorScreenPos(pos);
					    ImGui::Dummy(ImVec2(squareSize, squareSize));

                        std::stringstream ss;

                        ss << "ID: " << i << '\n';
                        ss << "Assigned Color: " << colInd;

			     		if (System::Current() == GBSystem::DMGCompatMode)
                            ss << "\nRGB5: " << (rgb5 & 0x1F) << " " << ((rgb5 >> 5) & 0x1F) << " " << ((rgb5 >> 10) & 0x1F);

                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", ss.str().c_str());
                    }
                };

                ImGui::SeparatorText("BGP");
                drawPalette(DMGPalette::BGP);

				ImGui::SeparatorText("OBP 0");
                drawPalette(DMGPalette::OBP0);

				ImGui::SeparatorText("OBP 1");
                drawPalette(DMGPalette::OBP1);
            }
        }
        ImGui::End();
    }
    if (showAudioView)
    {
        if (ImGui::Begin("Audio", &showAudioView, ImGuiWindowFlags_NoResize))
        {
            ImGui::SeparatorText("Channel Options");

            for (int i = 0; i < 4; i++)
            {
                const auto channelStr { "Channel " + std::to_string(i + 1) };
                bool tempFlag { gb.apu.enabledChannels[i].load() };

                if (ImGui::Checkbox(channelStr.c_str(), &tempFlag))
                    gb.apu.enabledChannels[i].store(tempFlag);
            }
        }

        ImGui::End();
    }
}