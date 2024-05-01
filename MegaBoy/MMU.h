#pragma once
#include <fstream>
#include <cstdint>
#include <array>

class GBCore;
class Cartridge;

class MMU
{
public:
	friend class Cartridge;
	MMU(GBCore& gbCore);

	void write8(uint16_t addr, uint8_t val);

	template <bool dmaBlocking = true>
	uint8_t read8(uint16_t addr) const;

	void executeDMA();

	inline void reset()
	{
		dmaTransfer = false;
		dmaRestartRequest = false;
		DMA = 0xFF;
	}

private:
	GBCore& gbCore;

	std::array<uint8_t, 8192> WRAM{};
	std::array<uint8_t, 127> HRAM{};
	std::array<uint8_t, 256> bootROM{};
	uint8_t DMA;

	bool dmaTransfer;
	bool dmaRestartRequest;
	uint8_t dmaCycles;
	uint16_t dmaSourceAddr;
	uint8_t dmaDelayCycles;

	void startDMATransfer();

	constexpr bool dmaInProgress() const { return dmaTransfer && dmaDelayCycles == 0; }
	static constexpr uint8_t DMA_CYCLES = 160;
};
