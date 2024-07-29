#pragma once
#include <fstream>
#include <cstdint>
#include <array>

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
	MMU(GBCore& gbCore);

	void write8(uint16_t addr, uint8_t val);

	template <bool dmaBlocking = true>
	uint8_t read8(uint16_t addr) const;

	void executeDMA();
	void executeGHDMA();

	inline void reset()
	{
		s = {};
	}

	void saveState(std::ofstream& st);
	void loadState(std::ifstream& st);

	struct DMAstate
	{
		uint8_t reg{ 0xFF };
		bool transfer{ false };
		bool restartRequest{ false };
		uint8_t cycles{ 0x00 };
		uint16_t sourceAddr{ 0x00 };
		uint8_t delayCycles{ 0x00 };
	};

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
	};

	struct MMUstate
	{
		bool statRegChanged{ false };
		uint8_t newStatVal{ 0 };
		DMAstate dma;
		GHDMAstate hdma;
		uint8_t wramBank{ 1 };
	};

	MMUstate s{};

	std::array<uint8_t, 256> base_bootROM{};
	std::array<uint8_t, 0x700> GBCbootROM{};
private:
	GBCore& gbCore;
	static constexpr uint8_t DMA_CYCLES = 160;

	std::array<uint8_t, 0x8000> WRAM_BANKS{};
	std::array<uint8_t, 127> HRAM{};

	constexpr bool dmaInProgress() const { return s.dma.transfer && s.dma.delayCycles == 0; }
	void startDMATransfer();
};
