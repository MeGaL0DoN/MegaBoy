#pragma once
#include "MMU.h"
#include "CPU.h"
#include "bitOps.h"
#include <array>
#include <vector>

#include "pixelOps.h"
using color = PixelOps::color;

enum class PPUMode : uint8_t
{
	HBlank = 0,
	VBlank = 1,
	OAMSearch = 2,
	PixelTransfer = 3,
};

struct object
{
	int16_t X;
	int16_t Y;
	uint16_t tileAddr;
	uint8_t attributes;

	static bool objComparator(const object& obj1, const object& obj2)
	{
		if (obj1.X > obj2.X)
			return true;
		else if (obj1.X < obj2.X)
			return false;

		return obj1.tileAddr < obj2.tileAddr;
	}
};

class PPU
{
public:
	friend class MMU;

	static constexpr uint8_t SCR_WIDTH = 160;
	static constexpr uint8_t SCR_HEIGHT = 144;

	static constexpr uint16_t TILES_WIDTH = 16 * 8;
	static constexpr uint16_t TILES_HEIGHT = 24 * 8;

	static constexpr uint32_t FRAMEBUFFER_SIZE = SCR_WIDTH * SCR_HEIGHT * 3;
	static constexpr uint32_t TILEDATA_FRAMEBUFFER_SIZE = TILES_WIDTH * TILES_HEIGHT * 3;

	void (*onBackgroundRender)(const uint8_t* buffer, uint8_t LY) { nullptr };
	void (*onWindowRender)(const uint8_t*, const std::vector<uint8_t>& updatedPixels, uint8_t LY) { nullptr };
	void (*onOAMRender)(const uint8_t* buffer, const std::vector<uint8_t>& updatedPixels, uint8_t LY) { nullptr };

	void (*drawCallback)(const uint8_t* framebuffer) { nullptr };

	constexpr void resetRenderCallbacks()
	{
		onBackgroundRender = nullptr;
		onWindowRender = nullptr;
		onOAMRender = nullptr;
	}

	PPU(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu)
	{
		std::memset(framebuffer.data(), 255, FRAMEBUFFER_SIZE); // White
	}

	void execute();
	void reset();

	constexpr const uint8_t* getFrameBuffer() { return framebuffer.data(); }
	void renderTileData(uint8_t* buffer, int vramBank);

	static constexpr std::array<color, 4> GRAY_PALETTE = { color {255, 255, 255}, color {169, 169, 169}, color {84, 84, 84}, color {0, 0, 0} };
	static constexpr std::array<color, 4> CLASSIC_PALETTE = { color {155, 188, 15}, color {139, 172, 15}, color {48, 98, 48}, color {15, 56, 15} };
	static constexpr std::array<color, 4> BGB_GREEN_PALETTE = { color {224, 248, 208}, color {136, 192, 112 }, color {52, 104, 86}, color{8, 24, 32} };

	constexpr void setDMGPalette(const std::array<color, 4>& newColors) { colors = newColors; }
	constexpr const std::array<color, 4>& getCurrentPalette() { return colors; }

	void updateDMG_ScreenColors(const std::array<color, 4>& newColors);

	constexpr const std::array<uint8_t, 160>& getOAM() { return OAM; }

	void saveState(std::ofstream& st);
	void loadState(std::ifstream& st);

	struct dmgRegs
	{
		uint8_t LCDC{ 0x91 };
		uint8_t STAT{ 0x85 };
		uint8_t SCY{ 0x00 };
		uint8_t SCX{ 0x00 };
		uint8_t LYC{ 0x00 };
		uint8_t BGP{ 0xFC };
		uint8_t OBP0{ 0x00 };
		uint8_t OBP1{ 0x00 };
		uint8_t WY{ 0x00 };
		uint8_t WX{ 0x00 };
	};

	struct addrPaletteReg
	{
		uint8_t value : 6 { 0x00 };
		bool autoIncrement { false };

		uint8_t read()
		{
			return (static_cast<uint8_t>(autoIncrement) << 7 | value) | 0x40;
		}

		void write(uint8_t val)
		{
			autoIncrement = getBit(val, 7);
			value = val & 0x3F;
		}
	};

	struct gbcRegs
	{
		uint8_t VBK{ 0xFE };
		addrPaletteReg BCPS{};
		addrPaletteReg OCPS{};
	};

	struct ppuState
	{
		bool lycFlag { false };
		bool blockStat { false };

		uint8_t LY { 0 };
		uint8_t WLY { 0 };

		uint16_t VBLANK_CYCLES;
		PPUMode state { PPUMode::OAMSearch };
		uint16_t videoCycles { 0 };
	};

	struct BGPixelFlag
	{
		bool opaque : 1 { false };
		bool gbcPriority : 1{ false };
	};

	ppuState s{};
	dmgRegs regs{};
	gbcRegs gbcRegs{};
private:
	MMU& mmu;
	CPU& cpu;

	uint8_t* VRAM { VRAM_BANK0.data() };
	std::array<uint8_t, 8192> VRAM_BANK0{};
	std::array<uint8_t, 8192> VRAM_BANK1{};

	std::array<uint8_t, 160> OAM{};

	std::array<uint8_t, 64> BGpaletteRAM{};
	std::array<uint8_t, 64> OBPpaletteRAM{};

	bool canAccessOAM{};
	bool canAccessVRAM{};

	std::array<uint8_t, FRAMEBUFFER_SIZE> framebuffer{};

	std::array<BGPixelFlag, SCR_WIDTH> bgPixelFlags{};

	std::vector<uint8_t> updatedWindowPixels{};
	std::vector<uint8_t> updatedOAMPixels{};

	uint8_t objCount { 0 };
	std::array<object, 10> selectedObjects{};

	static constexpr uint16_t OAM_SCAN_CYCLES = 20 * 4;
	static constexpr uint16_t PIXEL_TRANSFER_CYCLES = 43 * 4;
	static constexpr uint16_t HBLANK_CYCLES = 51 * 4;
	static constexpr uint16_t DEFAULT_VBLANK_CYCLES = 114 * 4;

	constexpr void invokeDrawCallback() { if (drawCallback != nullptr) drawCallback(framebuffer.data()); }

	inline void clearBuffer()
	{
		PixelOps::clearBuffer(framebuffer.data(), SCR_WIDTH, SCR_HEIGHT, System::Current() == GBSystem::DMG ? colors[0] : color { 255, 255, 255 });
		invokeDrawCallback();
	}

	std::array<color, 4> colors { };
	constexpr color getColor(uint8_t ind) { return colors[ind < colors.size() ? ind : 0]; }

	constexpr void setPixel(uint8_t x, uint8_t y, color c) { PixelOps::setPixel(framebuffer.data(), SCR_WIDTH, x, y, c); }
	constexpr color getPixel(uint8_t x, uint8_t y) { return PixelOps::getPixel(framebuffer.data(), SCR_WIDTH, x, y); }

	std::array<uint8_t, 4> BGpalette{};
	std::array<uint8_t, 4> OBP0palette{};
	std::array<uint8_t, 4> OBP1palette{};

	void updatePalette(uint8_t val, std::array<uint8_t, 4>& palette);

	void checkLYC();
	void requestSTAT();
	void updateInterrupts();

	void SetPPUMode(PPUMode ppuState);
	void disableLCD(PPUMode mode = PPUMode::HBlank);

	void handleOAMSearch();
	void handleHBlank();
	void handleVBlank();
	void handlePixelTransfer();

	void renderScanLine();
	void renderBackground();
	void renderWindow();
	void renderOAM();
	void DMG_renderBlank();

	inline void setVRAMBank(uint8_t val)
	{
		VRAM = val & 0x1 ? VRAM_BANK1.data() : VRAM_BANK0.data();
		gbcRegs.VBK = 0xFE | val;
	}
	inline void writePaletteRAM(std::array<uint8_t, 64>& ram, addrPaletteReg& addr, uint8_t val)
	{
		ram[addr.value] = val;
		if (addr.autoIncrement) addr.value++;
	}

	template <bool updateWindowChangesBuffer>
	void DMG_renderBGTile(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);
	void DMG_renderObjTile(const object& obj);

	template <bool updateWindowChangesBuffer>
	void GBC_renderBGTile(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);
	void GBC_renderObjTile(const object& obj);

	void(PPU::*renderWinTileFunc)(uint16_t, int16_t, uint8_t) { nullptr };
	void(PPU::*renderBGTileFunc)(uint16_t, int16_t, uint8_t) { nullptr };
	void(PPU::*renderObjTileFunc)(const object&) { nullptr };

	void updateFunctionPointers();

	inline uint16_t getBGTileAddr(uint8_t tileInd) { return BGUnsignedAddressing() ? tileInd * 16 : 0x1000 + static_cast<int8_t>(tileInd) * 16; }

	inline bool GBCMasterPriority() { return !getBit(regs.LCDC, 0); }
	inline bool TileMapsEnable() { return System::Current() == GBSystem::GBC || getBit(regs.LCDC, 0); }

	inline bool OBJEnable() { return getBit(regs.LCDC, 1); }
	inline bool DoubleOBJSize() { return getBit(regs.LCDC, 2); }
	inline uint16_t BGTileAddr() { return getBit(regs.LCDC, 3) ? 0x1C00 : 0x1800; }
	inline uint16_t WindowTileAddr() { return getBit(regs.LCDC, 6) ? 0x1C00 : 0x1800; }
	inline bool BGUnsignedAddressing() { return getBit(regs.LCDC, 4); }
	inline bool WindowEnable() { return getBit(regs.LCDC, 5); }
	inline bool LCDEnabled() { return getBit(regs.LCDC, 7); }

	inline bool LYC_STAT() { return getBit(regs.STAT, 6); }
	inline bool OAM_STAT() { return getBit(regs.STAT, 5); }
	inline bool VBlank_STAT() { return getBit(regs.STAT, 4); }
	inline bool HBlank_STAT() { return getBit(regs.STAT, 3); }
};