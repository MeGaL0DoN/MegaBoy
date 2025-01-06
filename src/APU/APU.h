#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>
#include <atomic>

#include "squareWave.h"
#include "sweepWave.h"
#include "customWave.h"
#include "noiseWave.h"

class GBCore;

struct globalAPURegs
{
	std::atomic<uint8_t> NR50, NR51;
	std::atomic<bool> apuEnable; // Instead of NR52, since it is the only writable bit.
};

class APU
{
public:
	friend class MMU;

	void reset();

	explicit APU(GBCore& gbCore);
	~APU();

	void execute(uint32_t cycles);
	std::pair<int16_t, int16_t> generateSamples();

	inline bool enabled() { return regs.apuEnable; }

	void saveState(std::ostream& st) const;
	void loadState(std::istream& st);

	static constexpr uint32_t CPU_FREQUENCY = 1053360;
	static constexpr uint32_t SAMPLE_RATE = 44100;
	static constexpr uint32_t CYCLES_PER_SAMPLE = CPU_FREQUENCY / SAMPLE_RATE;
	static constexpr uint16_t CHANNELS = 2;

	std::atomic<float> volume { 0.5 };
	std::array<std::atomic<bool>, 4> enabledChannels { true, true, true, true };

	bool recording{ false };
	void startRecording(const std::filesystem::path& filePath);
	void stopRecording();

	std::ofstream recordingStream;
	std::vector<int16_t> recordingBuffer;
private:
	void executeFrameSequencer();
	void initMiniAudio();
	void writeWAVHeader();

	typedef class ma_device ma_device;
	std::unique_ptr<ma_device> soundDevice;

	GBCore& gb;

	sweepWave channel1{};
	squareWave<> channel2{};
	customWave channel3{};
	noiseWave channel4{};

	globalAPURegs regs{};

	inline uint8_t getNR52()
	{
		uint8_t NR52 = setBit(0x70, 7, regs.apuEnable);
		NR52 = setBit(NR52, 3, channel4.s.enabled);
		NR52 = setBit(NR52, 2, channel3.s.enabled);
		NR52 = setBit(NR52, 1, channel2.s.enabled);
		NR52 = setBit(NR52, 0, channel1.s.enabled);
		return NR52;
	}

	uint16_t frameSequencerCycles{};
	uint8_t frameSequencerStep{};
};