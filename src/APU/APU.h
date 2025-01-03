#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>

#include "squareWave.h"
#include "sweepWave.h"

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

	int16_t getSample() const { return sample * INT16_MAX; }

	static constexpr uint32_t CPU_FREQUENCY = 1053360;
	static constexpr uint32_t SAMPLE_RATE = 44100;
	static constexpr uint32_t CYCLES_PER_SAMPLE = CPU_FREQUENCY / SAMPLE_RATE;

	static constexpr uint16_t CHANNELS = 2;
	static constexpr uint16_t BITS_PER_SAMPLE = sizeof(int16_t) * CHAR_BIT;

	float volume { 1.0 };

	bool enableChannel1 { true };
	bool enableChannel2 { true };
	bool enableChannel3 { true };
	bool enableChannel4 { true };

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

	GBCore& gbCore;

	float sample{};

	sweepWave channel1 {};
	squareWave channel2 {};

	uint16_t frameSequencerCycles{};
	uint8_t frameSequencerStep{};
};