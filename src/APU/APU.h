#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>

#include "squareWave.h"
#include "sweepWave.h"
#include "customWave.h"

class GBCore;

class APU
{
public:
	friend class MMU;

	void reset();

	explicit APU(GBCore& gbCore);
	~APU();

	void execute();
	void generateSample();

	uint8_t getSample() const { return (sample * volume) * 255; }

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

	uint8_t NR50{};
private:
	void executeFrameSequencer();
	void initMiniAudio();
	void writeWAVHeader();

	typedef class ma_device ma_device;
	std::unique_ptr<ma_device> soundDevice;

	GBCore& gb;

	float sample{};

	sweepWave channel1{};
	squareWave<> channel2{};
	customWave channel3{};

	uint16_t frameSequencerCycles{};
	uint8_t frameSequencerStep{};
};