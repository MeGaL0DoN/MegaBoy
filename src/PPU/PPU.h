#pragma once

#include <cstdint>
#include <array>
#include <fstream>

#include "../defines.h"
#include "../Utils/pixelOps.h"
#include "../Utils/bitOps.h"

using color = PixelOps::color;

enum class PPUMode : uint8_t
{
	HBlank = 0,
	VBlank = 1,
	OAMSearch = 2,
	PixelTransfer = 3,
};

enum class FetcherState : uint8_t
{
	FetchTileNo,
	FetchTileDataLow,
	FetchTileDataHigh,
	PushFIFO
};

struct FIFOEntry
{
	uint8_t color : 2  {};
	uint8_t palette : 3 {};
	bool priority : 1 {};
};

struct FIFOState
{
	uint8_t cycles{ 0 };
	FetcherState state{ FetcherState::FetchTileNo };

	uint8_t tileLow{};
	uint8_t tileHigh{};
};
struct BGFIFOState : FIFOState
{
	uint16_t tileMap{};
	uint8_t fetchX{ 0 };
	uint8_t cgbAttributes{};

	int8_t scanlineDiscardPixels { -1 };
	bool newScanline{ true };
	bool fetchingWindow{ false };
};
struct ObjFIFOState : FIFOState
{
	uint8_t objInd { 0 };
	bool fetchRequested { false };
	bool fetcherActive { false };
};

template <typename T>
struct PixelFIFO
{
	std::array<FIFOEntry, 8> data{};
	T s;

	inline void push(FIFOEntry ent)
	{
		data[back++] = ent;
		back &= 0x7;
		size++;
	}
	inline FIFOEntry pop()
	{
		const auto val = data[front++];
		front &= 0x7;
		size--;
		return val;
	}

	inline FIFOEntry& operator[](uint8_t ind)
	{
		return data[(front + ind) & 0x7];
	}

	inline bool full() const { return size == 8; };
	inline bool empty() const { return size == 0; };

	inline void clear() { front = 0; back = 0; size = 0; }

	inline void reset()
	{
		s = {};
		clear();
	}

	inline void saveState(std::ofstream& st)
	{
		ST_WRITE(s);
		ST_WRITE(front);
		ST_WRITE(back);
		ST_WRITE(size);
		ST_WRITE_ARR(data);
	}
	inline void loadState(std::ifstream& st)
	{
		ST_READ(s);
		ST_READ(front);
		ST_READ(back);
		ST_READ(size);
		ST_READ_ARR(data);
	}
protected:
	uint8_t front{ 0 };
	uint8_t back{ 0 };
	uint8_t size{ 0 };
};

struct BGPixelFIFO : PixelFIFO<BGFIFOState>
{ };

struct ObjPixelFIFO : PixelFIFO<ObjFIFOState>
{};

struct OAMobject
{
	int16_t X{};
	int16_t Y{};
	uint16_t tileAddr{};
	uint8_t attributes{};
	uint8_t oamAddr{};
};

struct ppuState
{
	bool lycFlag{ false };
	bool blockStat{ false };
	bool lcdWasEnabled { false };

	uint8_t LY{ 0 };
	uint8_t WLY{ 0 };
	uint8_t xPosCounter{ 0 };

	uint16_t VBLANK_CYCLES{};
	uint16_t HBLANK_CYCLES{};
	PPUMode state{ PPUMode::OAMSearch };
	uint16_t videoCycles{ 0 };
};

struct ppuGBCPaletteData
{
	std::array<uint8_t, 64> paletteRAM{};
	uint8_t regValue{ 0x00 };
	bool autoIncrement{ false };

	inline uint8_t readReg() const
	{
		return (static_cast<uint8_t>(autoIncrement) << 7 | regValue) | 0x40;
	}
	inline void writeReg(uint8_t val)
	{
		autoIncrement = getBit(val, 7);
		regValue = val & 0x3F;
	}

	inline uint8_t readPaletteRAM() const
	{
		return paletteRAM[regValue];
	}
	inline void writePaletteRAM(uint8_t val)
	{
		paletteRAM[regValue] = val;
		regValue = autoIncrement ? ((regValue + 1) & 0x3F) : regValue;
	}

	inline void loadState(std::ifstream& st)
	{
		ST_READ_ARR(paletteRAM);
		ST_READ(regValue);
		ST_READ(autoIncrement);
	}
	inline void saveState(std::ofstream& st) const
	{
		ST_WRITE_ARR(paletteRAM);
		ST_WRITE(regValue);
		ST_WRITE(autoIncrement);
	}
};

struct ppuGBCRegs
{
	uint8_t VBK{ 0xFE };
	ppuGBCPaletteData BCPS{};
	ppuGBCPaletteData OCPS{};
};

struct ppuDMGRegs
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

class PPU
{
public:
	static constexpr uint8_t SCR_WIDTH = 160;
	static constexpr uint8_t SCR_HEIGHT = 144;
	static constexpr uint32_t FRAMEBUFFER_SIZE = SCR_WIDTH * SCR_HEIGHT * 3;

	static constexpr uint16_t TILES_WIDTH = 16 * 8;
	static constexpr uint16_t TILES_HEIGHT = 24 * 8;
	static constexpr uint32_t TILEDATA_FRAMEBUFFER_SIZE = TILES_WIDTH * TILES_HEIGHT * 3;

	static constexpr uint16_t TILEMAP_WIDTH = 32 * 8;
	static constexpr uint16_t TILEMAP_HEIGHT = 32 * 8;
	static constexpr uint32_t TILEMAP_FRAMEBUFFER_SIZE = TILEMAP_WIDTH * TILEMAP_HEIGHT * 3;

	static constexpr std::array<color, 4> GRAY_PALETTE = { color {255, 255, 255}, color {169, 169, 169}, color {84, 84, 84}, color {0, 0, 0} };
	static constexpr std::array<color, 4> CLASSIC_PALETTE = { color {155, 188, 15}, color {139, 172, 15}, color {48, 98, 48}, color {15, 56, 15} };
	static constexpr std::array<color, 4> BGB_GREEN_PALETTE = { color {224, 248, 208}, color {136, 192, 112 }, color {52, 104, 86}, color{8, 24, 32} };

	// MIST GB Palette: https://lospec.com/palette-list/mist-gb
	static constexpr std::array<color, 4> DEFAULT_CUSTOM_PALETTE { color {196, 240, 194}, color {90, 185, 168}, color {30, 96, 110}, color {45, 27, 0} };
	static inline std::array<color, 4> CUSTOM_PALETTE { DEFAULT_CUSTOM_PALETTE };

	static inline const color* ColorPalette { };

	friend class MMU;

	virtual ~PPU() = default;

	virtual void execute(uint8_t cycles) = 0;
	virtual void reset() = 0;

	virtual void setLCDEnable(bool val) = 0;

	virtual void saveState(std::ofstream& st) = 0;
	virtual void loadState(std::ifstream& st) = 0;

	virtual void refreshDMGScreenColors(const std::array<color, 4>& newColorPalette) = 0;

	virtual void renderTileData(uint8_t* buffer, int vramBank) = 0;
	virtual void renderBGTileMap(uint8_t* buffer) = 0;
	virtual void renderWindowTileMap(uint8_t* buffer) = 0;

	constexpr const uint8_t* getFrameBuffer() const { return framebuffer.data(); }
	void (*drawCallback)(const uint8_t* framebuffer) { nullptr };

	inline uint8_t* oamFramebuffer() { return debugOAMFramebuffer.get(); }
	inline void setOAMDebugEnable(bool val)
	{
		debugOAM = val;

		if (val && !debugOAMFramebuffer) 
			debugOAMFramebuffer = std::make_unique<uint8_t[]>(FRAMEBUFFER_SIZE);
	}
protected:
	std::array<uint8_t, FRAMEBUFFER_SIZE> framebuffer{};

	std::array<uint8_t, 160> OAM{};
	std::array<uint8_t, 8192> VRAM_BANK0{};
	std::array<uint8_t, 8192> VRAM_BANK1{};

	uint8_t* VRAM { VRAM_BANK0.data() };

	std::array<uint8_t, 4> BGpalette{};
	std::array<uint8_t, 4> OBP0palette{};
	std::array<uint8_t, 4> OBP1palette{};

	uint8_t objCount{ 0 };
	std::array<OAMobject, 10> selectedObjects{};

	ppuDMGRegs regs{};
	ppuGBCRegs gbcRegs{};

	ppuState s{};
	BGPixelFIFO bgFIFO{};
	ObjPixelFIFO objFIFO{};

	bool canAccessOAM{};
	bool canAccessVRAM{};

	bool debugOAM{ false };
	std::unique_ptr<uint8_t[]> debugOAMFramebuffer{};

	inline static void updatePalette(uint8_t val, std::array<uint8_t, 4>& palette)
	{
		for (uint8_t i = 0; i < 4; i++)
			palette[i] = (getBit(val, i * 2 + 1) << 1) | getBit(val, i * 2);
	}

	inline void setVRAMBank(uint8_t val)
	{
		VRAM = val & 0x1 ? VRAM_BANK1.data() : VRAM_BANK0.data();
		gbcRegs.VBK = 0xFE | val;
	}
};