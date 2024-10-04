#pragma once
#include <array>
#include <vector>

#include "PPU.h"
#include "MMU.h"
#include "CPU/CPU.h"

template <GBSystem sys>
class PPUCore : public PPU
{
public:
	//void (*onBackgroundRender)(const uint8_t* buffer, uint8_t LY) { nullptr };
	//void (*onWindowRender)(const uint8_t*, const std::vector<uint8_t>& updatedPixels, uint8_t LY) { nullptr };
	//void (*onOAMRender)(const uint8_t* buffer, const std::vector<uint8_t>& updatedPixels, uint8_t LY) { nullptr };

	//constexpr void resetRenderCallbacks()
	//{
	//	onBackgroundRender = nullptr;
	//	onWindowRender = nullptr;
	//	onOAMRender = nullptr;
	//}

	PPUCore(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu) { }

	void execute() override;
	void reset() override;

	void saveState(std::ofstream& st) override;
	void loadState(std::ifstream& st) override;

	void renderTileData(uint8_t* buffer, int vramBank) override;
	void refreshDMGScreenColors(const std::array<color, 4>& newColors) override;
private:
	MMU& mmu;
	CPU& cpu;

	static constexpr uint16_t TOTAL_SCANLINE_CYCLES = 456;
	static constexpr uint16_t OAM_SCAN_CYCLES = 20 * 4;
	static constexpr uint16_t DEFAULT_VBLANK_CYCLES = 114 * 4;

	constexpr void invokeDrawCallback() { if (drawCallback != nullptr) drawCallback(framebuffer.data()); }

	inline void clearBuffer()
	{
		PixelOps::clearBuffer(framebuffer.data(), SCR_WIDTH, SCR_HEIGHT, System::Current() == GBSystem::DMG ? PPU::ColorPalette[0] : color { 255, 255, 255 });
		invokeDrawCallback();
	}

	void checkLYC();
	void requestSTAT();
	void updateInterrupts();

	void SetPPUMode(PPUMode ppuState);
	void disableLCD(PPUMode mode) override;

	void handleOAMSearch();
	void handleHBlank();
	void handleVBlank();

	void handlePixelTransfer();
	void resetPixelTransferState();

	void tryStartSpriteFetcher();
	void executeBGFetcher();
	void executeObjFetcher();
	void renderFIFOs();

	constexpr color getPixel(uint8_t x, uint8_t y) { return PixelOps::getPixel(framebuffer.data(), SCR_WIDTH, x, y); }
	constexpr void setPixel(uint8_t x, uint8_t y, color c) { PixelOps::setPixel(framebuffer.data(), SCR_WIDTH, x, y, c); }

	constexpr uint8_t getColorID(uint8_t tileLow, uint8_t tileHigh, uint8_t ind) { return ((tileHigh >> ind) & 1) << 1 | ((tileLow >> ind) & 1); }

	template <bool obj>
	constexpr color getColor(uint8_t colorID, uint8_t palette)
	{
		if constexpr (sys == GBSystem::DMG)
		{
			uint8_t* palettePtr;

			if constexpr (obj)
				palettePtr = palette == 0 ? OBP0palette.data() : OBP1palette.data();
			else
				palettePtr = BGpalette.data();

			return PPU::ColorPalette[palettePtr[colorID]];
		}
		else
		{
			const uint8_t* paletteRamPtr;

			if constexpr (obj)
				paletteRamPtr = OBPpaletteRAM.data();
			else
				paletteRamPtr = BGpaletteRAM.data();

			const uint8_t paletteRAMInd = palette * 8 + colorID * 2;
			return color::fromRGB5(paletteRamPtr[paletteRAMInd + 1] << 8 | paletteRamPtr[paletteRAMInd]);
		}
	}

	inline uint16_t getBGTileAddr(uint8_t tileInd) { return BGUnsignedAddressing() ? tileInd * 16 : 0x1000 + static_cast<int8_t>(tileInd) * 16; }

	inline uint8_t getBGTileOffset()
	{
		const uint8_t bgLineOffset = (s.LY + regs.SCY) % 8;
		const uint8_t windowLineOffset = s.WLY % 8;

		if constexpr (sys == GBSystem::DMG)
			return 2 * (bgFIFO.s.fetchingWindow ? windowLineOffset : bgLineOffset);
		else
		{
			const bool yFlip = getBit(bgFIFO.s.cgbAttributes, 6);
			return 2 * (bgFIFO.s.fetchingWindow ? (yFlip ? 7 - windowLineOffset : windowLineOffset) : (yFlip ? 7 - bgLineOffset : bgLineOffset));
		}
	}
	inline uint8_t getObjTileOffset(const OAMobject& obj)
	{
		const bool yFlip = getBit(obj.attributes, 6);
		return static_cast<uint8_t>(2 * (yFlip ? (obj.Y - s.LY + 7) : (8 - (obj.Y - s.LY + 8))));
	}

	inline bool GBCMasterPriority() { return !getBit(regs.LCDC, 0); }
	inline bool DMGTileMapsEnable() { return getBit(regs.LCDC, 0); }

	inline bool OBJEnable() { return getBit(regs.LCDC, 1); }
	inline bool DoubleOBJSize() { return getBit(regs.LCDC, 2); }
	inline uint16_t BGTileMapAddr() { return getBit(regs.LCDC, 3) ? 0x1C00 : 0x1800; }
	inline uint16_t WindowTileMapAddr() { return getBit(regs.LCDC, 6) ? 0x1C00 : 0x1800; }
	inline bool BGUnsignedAddressing() { return getBit(regs.LCDC, 4); }
	inline bool WindowEnable() { return getBit(regs.LCDC, 5); }
	inline bool LCDEnabled() { return getBit(regs.LCDC, 7); }

	inline bool LYC_STAT() { return getBit(regs.STAT, 6); }
	inline bool OAM_STAT() { return getBit(regs.STAT, 5); }
	inline bool VBlank_STAT() { return getBit(regs.STAT, 4); }
	inline bool HBlank_STAT() { return getBit(regs.STAT, 3); }
};