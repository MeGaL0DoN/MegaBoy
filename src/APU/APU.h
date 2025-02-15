#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <filesystem>
#include <fstream>
#include <atomic>

#include "squareWave.h"
#include "sweepWave.h"
#include "customWave.h"
#include "noiseWave.h"

struct globalAPURegs
{
	std::atomic<uint8_t> NR50, NR51;
	std::atomic<bool> apuEnable; // Instead of NR52, since it is the only writable bit.
};

class GBCore;

class APU
{
public:
	friend class MMU;
	friend GBCore;

	void reset();

	explicit APU(GBCore& gbCore);
	~APU();

	void execute(int cycles);
	std::pair<int16_t, int16_t> generateSamples();

	inline bool enabled() { return regs.apuEnable; }

	void saveState(std::ostream& st) const;
	void loadState(std::istream& st);

	static constexpr uint32_t CPU_FREQUENCY = 1048576;
	static constexpr uint32_t SAMPLE_RATE = 48000;
	static constexpr double CYCLES_PER_SAMPLE = static_cast<double>(CPU_FREQUENCY) / SAMPLE_RATE;
	static constexpr uint16_t CHANNELS = 2;

	std::atomic<float> volume { 0.5 };
	std::array<std::atomic<bool>, 4> enabledChannels { true, true, true, true };

	std::atomic<bool> isRecording { false };
	std::atomic<float> recordedSeconds { 0.f };

	static inline std::atomic<bool> IsMainThreadBlocked { false };
	static inline std::atomic<double> LastMainThreadTime { 0.0 };

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

	inline uint8_t getNR52() const
	{
		uint8_t NR52 = setBit(0x70, 7, regs.apuEnable);
		NR52 = setBit(NR52, 3, channel4.s.enabled);
		NR52 = setBit(NR52, 2, channel3.s.enabled);
		NR52 = setBit(NR52, 1, channel2.s.enabled);
		NR52 = setBit(NR52, 0, channel1.s.enabled);
		return NR52;
	}

	inline uint8_t readPCM12()
	{
		return (channel2.getSample() << 4) | channel1.getSample();
	}
	inline uint8_t readPCM34()
	{
		return (channel4.getSample() << 4) | channel3.getSample();
	}

	uint16_t frameSequencerCycles{};
	uint8_t frameSequencerStep{};
};