#pragma once
#include <fstream>
#include <cstdint>
#include <array>

class GBCore;
class Cartridge;

class MMU
{
public:
	MMU(GBCore& gbCore);

	void write8(uint16_t addr, uint8_t val);

	template <bool dmaBlocking = true>
	uint8_t read8(uint16_t addr) const;

	void executeDMA();

	inline void reset()
	{
		s.dma.transfer = false;
		s.dma.restartRequest = false;
		s.dma.reg = 0xFF;
	}

	void saveState(std::ofstream& st);
	void loadState(std::ifstream& st);

	struct DMAstate
	{
		uint8_t reg;
		bool transfer;
		bool restartRequest;
		uint8_t cycles;
		uint16_t sourceAddr;
		uint8_t delayCycles;
	};

	struct MMUstate
	{
		bool statRegChanged{ false };
		uint8_t newStatVal{ 0 };
		DMAstate dma;
	};

	MMUstate s{};

	std::array<uint8_t, 256> bootROM{};

private:
	GBCore& gbCore;
	static constexpr uint8_t DMA_CYCLES = 160;

	std::array<uint8_t, 8192> WRAM{};
	std::array<uint8_t, 127> HRAM{};

	constexpr bool dmaInProgress() const { return s.dma.transfer && s.dma.delayCycles == 0; }
	void startDMATransfer();
};
