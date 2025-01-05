#pragma once
#include <cstdint>
#include <array>
#include <iostream>
#include "gbSystem.h"

class GBCore;
class Cartridge;

enum class GHDMAStatus
{
	None,
	GDMA,
	HDMA,
};

class MMU
{
public:
	explicit MMU(GBCore& gbCore);
	void updateFunctionPointers();

	inline void write8(uint16_t addr, uint8_t val) { (this->*write_func)(addr, val); }
	inline uint8_t read8(uint16_t addr) const { return (this->*read_func)(addr); }

	void execute();

	void executeDMA();
	void executeGHDMA();

	inline void reset()
	{
		s = {};
		updateFunctionPointers();
	}

	void saveState(std::ostream& st) const;
	void loadState(std::istream& st);

	struct DMAstate
	{
		uint8_t reg{ 0xFF };
		bool transfer{ false };
		bool restartRequest{ false };
		uint8_t cycles{ 0x00 };
		uint16_t sourceAddr{ 0x00 };
		uint8_t delayCycles{ 0x00 };
	};

	static constexpr uint8_t DMA_CYCLES = 160;
	static constexpr uint8_t GHDMA_BLOCK_CYCLES = 32;

	struct GHDMAstate
	{
		uint16_t sourceAddr{ 0xFF };
		uint16_t destAddr{ 0xFF };

		uint16_t currentSourceAddr{ 0x00 };
		uint16_t currentDestAddr { 0x00 };

		uint16_t transferLength{ 0xFF };
		uint8_t cycles{ 0x00 };
		GHDMAStatus status { GHDMAStatus::None };
		bool active { false };
	};

	struct DMGstate
	{
		bool statRegChanged{ false };
		uint8_t newStatVal{ 0 };
		DMAstate dma{};
	};

	struct GBCState
	{
		GHDMAstate ghdma{};
		uint8_t wramBank{ 1 };

		// undocumented CGB registers
		uint8_t FF72{ 0x00 }, FF73{ 0x00 }, FF74{ 0x00 }, FF75 { 0x8F };
	};

	DMGstate s{};
	GBCState gbc{};

	std::array<uint8_t, 256> baseBootROM{};
	std::array<uint8_t, 0x700> cgbBootROM{};
private:
	GBCore& gb;

	std::array<uint8_t, 0x8000> WRAM_BANKS{};
	std::array<uint8_t, 127> HRAM{};

	constexpr bool dmaInProgress() const { return s.dma.transfer && s.dma.delayCycles == 0; }
	void startDMATransfer();

	template <GBSystem sys>
	void write8(uint16_t addr, uint8_t val);

	template <GBSystem sys, bool dmaBlocking>
	uint8_t read8(uint16_t addr) const;

	void(MMU::*write_func)(uint16_t, uint8_t) { nullptr };
	uint8_t(MMU::*read_func)(uint16_t) const { nullptr };
	uint8_t(MMU::*dma_nonblocking_read)(uint16_t) const { nullptr };
};
