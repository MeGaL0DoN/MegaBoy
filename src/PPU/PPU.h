#pragma once

#include <cstdint>
#include <array>
#include <iostream>
#include <memory>
#include <functional>
#include <random>

#include "../gbSystem.h"
#include "../defines.h"
#include "../Utils/pixelOps.h"
#include "../Utils/bitOps.h"
#include "../Utils/rngOps.h"

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

	inline void saveState(std::ostream& st) const
	{
		ST_WRITE(s);
		ST_WRITE(front);
		ST_WRITE(back);
		ST_WRITE(size);
		ST_WRITE_ARR(data);
	}
	inline void loadState(std::istream& st)
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
	bool lyIncremented{};
	bool blockStat{};
	bool lcdSkipFrame{};
	bool latchWindowEnable{};
	bool cgbVBlankFlag{};

	uint8_t LY{ 0 };
	uint8_t WLY{ 0 };
	uint8_t xPosCounter{ 0 };

	uint16_t vblankLineCycles{};
	uint16_t hblankCycles{};
	PPUMode state{ PPUMode::OAMSearch };
	PPUMode prevState{};
	uint16_t videoCycles{ 0 };
	uint32_t dotsUntilVBlank{};
};

struct ppuGBCPaletteData
{
	std::array<uint8_t, 64> paletteRAM{};
	uint8_t regValue{ 0x00 };
	bool autoIncrement{ false };

	static constexpr std::array<uint8_t, 8> DEFAULT_DMG_COMPAT_BG_PALETTE = { 255, 127, 239, 27, 128, 97, 0, 0 };
	static constexpr std::array<uint8_t, 16> DEFAULT_DMG_COMPAT_OBJ_PALETTE = { 255, 127, 31, 66, 242, 28, 0, 0, 255, 127, 31, 66, 242, 28, 0, 0 };

	// BCPS (bg) palette is set to white by default (0xFF -> 0x7F pattern, bit 7 of first byte is zero), OCPS (obj) is random.
	inline void reset(bool obj)
	{
		int i = 0;

		if (System::Current() == GBSystem::DMGCompatMode)
		{
			std::memcpy(paletteRAM.data(), obj ? DEFAULT_DMG_COMPAT_OBJ_PALETTE.data() : DEFAULT_DMG_COMPAT_BG_PALETTE.data(), obj ? 16 : 8);
			i = obj ? 16 : 8;
		}

		for (; i < paletteRAM.size(); i++)
			paletteRAM[i] = obj ? RngOps::gen8bit() : ((i & 1) == 0 ? 0xFF : 0x7F);
	}

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

	inline void loadState(std::istream& st)
	{
		ST_READ_ARR(paletteRAM);
		ST_READ(regValue);
		ST_READ(autoIncrement);
	}
	inline void saveState(std::ostream& st) const
	{
		ST_WRITE_ARR(paletteRAM);
		ST_WRITE(regValue);
		ST_WRITE(autoIncrement);
	}
};

struct ppuGBCRegs
{
	uint8_t VBK{};
	ppuGBCPaletteData BCPS{};
	ppuGBCPaletteData OCPS{};

	inline void reset()
	{
		VBK = 0xFE;
		BCPS.reset(false);
		OCPS.reset(true);
	}

	inline void saveState(std::ostream& st) const
	{
		ST_WRITE(VBK);
		BCPS.saveState(st);
		OCPS.saveState(st);
	}
	inline void loadState(std::istream& st)
	{
		ST_READ(VBK);
		BCPS.loadState(st);
		OCPS.loadState(st);
	}
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
	friend class debugUI;
	friend class MMU;

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

	static constexpr std::array GRAY_PALETTE = { color {255, 255, 255}, color {169, 169, 169}, color {84, 84, 84}, color {0, 0, 0} };
	static constexpr std::array CLASSIC_PALETTE = { color {155, 188, 15}, color {139, 172, 15}, color {48, 98, 48}, color {15, 56, 15} };
	static constexpr std::array BGB_GREEN_PALETTE = { color {224, 248, 208}, color {136, 192, 112 }, color {52, 104, 86}, color{8, 24, 32} };

	// MIST GB Palette: https://lospec.com/palette-list/mist-gb
	static constexpr std::array DEFAULT_CUSTOM_PALETTE { color {196, 240, 194}, color {90, 185, 168}, color {30, 96, 110}, color {45, 27, 0} };
	static inline std::array CUSTOM_PALETTE { DEFAULT_CUSTOM_PALETTE };

	static inline const color* ColorPalette { };

	virtual ~PPU() = default;

	std::function<void(const uint8_t*, bool)> drawCallback { nullptr };

	virtual void execute(uint8_t cycles) = 0;
	virtual void reset(bool clearBuf) = 0;

	virtual void setLCDEnable(bool val) = 0;

	virtual void saveState(std::ostream& st) const = 0;
	virtual void loadState(std::istream& st) = 0;

	virtual void refreshDMGScreenColors(const std::array<color, 4>& newColorPalette) = 0;

	virtual void renderTileData(uint8_t* buffer, int vramBank) = 0;
	virtual void renderBGTileMap(uint8_t* buffer) = 0;
	virtual void renderWindowTileMap(uint8_t* buffer) = 0;

	inline uint8_t* framebufferPtr() { return framebuffer.get(); }
	inline uint8_t* backbufferPtr() { return backbuffer.get(); }

	inline uint8_t* oamFramebuffer() { return debugOAMFramebuffer.get(); }
	inline uint8_t* bgFramebuffer() { return debugBGFramebuffer.get(); }
	inline uint8_t* windowFramebuffer() { return debugWindowFramebuffer.get(); }

	inline void setDebugEnable(bool val)
	{
		debugPPU = val;

		if (val && !debugOAMFramebuffer)
		{
			debugOAMFramebuffer = std::make_unique<uint8_t[]>(FRAMEBUFFER_SIZE);
			debugBGFramebuffer = std::make_unique<uint8_t[]>(FRAMEBUFFER_SIZE);
			debugWindowFramebuffer = std::make_unique<uint8_t[]>(FRAMEBUFFER_SIZE);
		}
	}
protected:
	std::unique_ptr<uint8_t[]> framebuffer { std::make_unique<uint8_t[]>(FRAMEBUFFER_SIZE) };
	std::unique_ptr<uint8_t[]> backbuffer { std::make_unique<uint8_t[]>(FRAMEBUFFER_SIZE) };

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

	bool debugPPU { false };
	std::unique_ptr<uint8_t[]> debugOAMFramebuffer{};
	std::unique_ptr<uint8_t[]> debugBGFramebuffer{};
	std::unique_ptr<uint8_t[]> debugWindowFramebuffer{};

	inline uint8_t readSTAT()
	{
		// Mode change is 1M cycle delayed.
		return (regs.STAT & (~0b11)) | static_cast<uint8_t>(s.prevState);
	}

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

	virtual bool canReadVRAM() = 0;
	virtual bool canReadOAM() = 0;
	virtual bool canWriteVRAM() = 0;
	virtual bool canWriteOAM() = 0;
};