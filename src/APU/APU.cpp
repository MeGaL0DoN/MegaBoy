#define MINIAUDIO_IMPLEMENTATION
#include <MiniAudio/miniaudio.h>
#include <GLFW/glfw3.h>

#include <cstring>
#include <thread>
#include "APU.h"
#include "../GBCore.h"
#include "../Utils/bitOps.h"
 
APU::APU(GBCore& gbCore) : gb(gbCore)
{}

APU::~APU()
{
	if (soundDevice != nullptr)
		ma_device_uninit(soundDevice.get());

	if (isRecording)
		stopRecording();
}

void APU::saveState(std::ostream& st) const
{
	ST_WRITE(regs);
	ST_WRITE(frameSequencerCycles);
	ST_WRITE(frameSequencerStep);
	ST_WRITE(channel1);
	ST_WRITE(channel2);
	ST_WRITE(channel3.s); ST_WRITE(channel3.regs); ST_WRITE_ARR(channel3.waveRAM);
	ST_WRITE(channel4);
}

void APU::loadState(std::istream& st)
{
	ST_READ(regs);
	ST_READ(frameSequencerCycles);
	ST_READ(frameSequencerStep);
	ST_READ(channel1);
	ST_READ(channel2);
	ST_READ(channel3.s); ST_READ(channel3.regs); ST_READ_ARR(channel3.waveRAM);
	ST_READ(channel4); 
}

void APU::reset()
{
	if (soundDevice == nullptr)
	{
#ifdef EMSCRIPTEN
		initMiniAudio();
#else
		std::thread t([this] { initMiniAudio(); }); // Because it can block main thread for a second or more.
		t.detach();
#endif
	}

	regs.NR50 = 0x77;
	regs.NR51 = 0xF3;
	regs.apuEnable = true;

	frameSequencerCycles = 0;
	frameSequencerStep = 0;

	channel1.reset();
	channel2.reset();
	channel3.reset();
	channel4.reset();
}

void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	auto& gb { *static_cast<GBCore*>(pDevice->pUserData) };
	auto& apu { gb.apu };
	auto* pOutput16 { static_cast<int16_t*>(pOutput) };

	const bool mainThreadBlocked { APU::isMainThreadBlocked || ((glfwGetTime() - APU::lastMainThreadTime) > 0.1) };
	const bool emulationStopped { gb.emulationPaused || gb.breakpointHit || !gb.cartridge.ROMLoaded() || mainThreadBlocked };

	if (!appConfig::enableAudio || emulationStopped || !apu.enabled())
	{
		if (!emulationStopped && apu.enabled())
			apu.execute(static_cast<int>(APU::CYCLES_PER_SAMPLE * frameCount));

		std::memset(pOutput16, 0, sizeof(int16_t) * frameCount * APU::CHANNELS);
		return;
	}

	constexpr int WHOLE_CYCLES { static_cast<int>(APU::CYCLES_PER_SAMPLE) };
	static double remainder { 0.0 };

	for (ma_uint32 i = 0; i < frameCount; i++)
	{
		const int cycles { static_cast<int>(WHOLE_CYCLES + remainder) };
		apu.execute(cycles);
		remainder += APU::CYCLES_PER_SAMPLE - cycles;

		const auto samples { apu.generateSamples() };

		pOutput16[i * 2] = samples.first;
		pOutput16[i * 2 + 1] = samples.second;
	}

	if (apu.isRecording)
	{
		const size_t bufferLen { apu.recordingBuffer.size() };
		const size_t newBufferLen { bufferLen + (frameCount * APU::CHANNELS) };

		apu.recordingBuffer.resize(newBufferLen);
		std::memcpy(&apu.recordingBuffer[bufferLen], pOutput16, sizeof(int16_t) * frameCount * APU::CHANNELS);

		if (newBufferLen >= APU::SAMPLE_RATE)
		{		
			apu.recordingStream.write(reinterpret_cast<char*>(apu.recordingBuffer.data()), newBufferLen * sizeof(int16_t));
			apu.recordingBuffer.clear();
		}

		apu.recordedSeconds += (static_cast<float>(frameCount) / APU::SAMPLE_RATE);
	}
}

void APU::startRecording(const std::filesystem::path& filePath)
{
	isRecording = true;
	recordingStream = std::ofstream { filePath, std::ios::binary };
	recordingBuffer.reserve(SAMPLE_RATE);
	writeWAVHeader();
}

#define WRITE(val) recordingStream.write(reinterpret_cast<const char*>(&val), sizeof(val));

void APU::writeWAVHeader()
{
	constexpr uint16_t BITS_PER_SAMPLE = sizeof(int16_t) * CHAR_BIT;
	constexpr uint32_t BYTE_RATE = SAMPLE_RATE * sizeof(int16_t) * CHANNELS;
	constexpr uint32_t SECTION_CHUNK_SIZE = 16;
	constexpr uint16_t BLOCK_ALIGN = (BITS_PER_SAMPLE * CHANNELS) / CHAR_BIT;
	constexpr uint16_t PCM_FORMAT = 1;

	recordingStream.write("RIFF", 4);

	uint32_t lengthReserve{};
	WRITE(lengthReserve);
	recordingStream.write("WAVE", 4);
	recordingStream.write("fmt ", 4);

	WRITE(SECTION_CHUNK_SIZE);
	WRITE(PCM_FORMAT);
	WRITE(CHANNELS);
	WRITE(SAMPLE_RATE);
	WRITE(BYTE_RATE);
	WRITE(BLOCK_ALIGN);
	WRITE(BITS_PER_SAMPLE);

	recordingStream.write("data", 4);

	uint32_t dataLengthReserve{};
	WRITE(dataLengthReserve);
}

void APU::stopRecording()
{
	isRecording = false;
	recordedSeconds = 0.f;

	if (!recordingBuffer.empty())
	{
		recordingStream.write(reinterpret_cast<char*>(recordingBuffer.data()), recordingBuffer.size() * sizeof(int16_t));
		recordingBuffer.clear();
	}

	recordingStream.seekp(0, std::ios::end);
	const uint32_t fileSize = static_cast<uint32_t>(recordingStream.tellp()) - 8;

	recordingStream.seekp(4, std::ios::beg);
	WRITE(fileSize);

	const uint32_t dataSize = fileSize - 36; // Header is 44 bytes, 44 - 8 = 36.
	recordingStream.seekp(40, std::ios::beg);
	WRITE(dataSize);

	recordingStream.close();
}

#undef WRITE

void APU::initMiniAudio()
{
	soundDevice = std::make_unique<ma_device>();

	ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = CHANNELS;
	deviceConfig.sampleRate = SAMPLE_RATE;
	deviceConfig.dataCallback = sound_data_callback;
	deviceConfig.pUserData = &gb;

	ma_device_init(NULL, &deviceConfig, soundDevice.get());
	ma_device_start(soundDevice.get());
}

void APU::executeFrameSequencer()
{
	frameSequencerCycles++;

	if (frameSequencerCycles == 2048)
	{
		if ((frameSequencerStep & 1) == 0)
		{
			channel1.executeLength();
			channel2.executeLength();
			channel3.executeLength();
			channel4.executeLength();

			if (frameSequencerStep == 2 || frameSequencerStep == 6)
				channel1.executeSweep();
		}
		else if (frameSequencerStep == 7)
		{
			channel1.executeEnvelope();
			channel2.executeEnvelope();
			channel4.executeEnvelope();
		}

		frameSequencerCycles = 0;
		frameSequencerStep = (frameSequencerStep + 1) & 7;
	}
}

void APU::execute(int cycles)
{
	while (cycles--)
	{
		channel1.execute();
		channel2.execute();
		channel3.execute();
		channel4.execute();

		executeFrameSequencer();
	}
}

std::pair<int16_t, int16_t> APU::generateSamples() 
{
	float leftSample { 0.f }, rightSample { 0.f };
	const uint8_t nr51 { regs.NR51.load() };

	const float sample1 = (channel1.getSample() / 15.f) * enabledChannels[0], sample2 = (channel2.getSample() / 15.f) * enabledChannels[1],
				sample3 = (channel3.getSample() / 15.f) * enabledChannels[2], sample4 = (channel4.getSample() / 15.f) * enabledChannels[3];

	leftSample += sample1 * getBit(nr51, 4);
	rightSample += sample1 * getBit(nr51, 0);

	leftSample += sample2 * getBit(nr51, 5);
	rightSample += sample2 * getBit(nr51, 1);

	leftSample += sample3 * getBit(nr51, 6);
	rightSample += sample3 * getBit(nr51, 2);

	leftSample += sample4 * getBit(nr51, 7);
	rightSample += sample4 * getBit(nr51, 3);

	const uint8_t leftVolume = ((regs.NR50 & 0x70) >> 4) + 1, rightVolume = (regs.NR50 & 0x7) + 1;
	const float scaleFactor { (volume * INT16_MAX) / 4.0f };

	leftSample *= scaleFactor * (leftVolume / 8.f);
	rightSample *= scaleFactor * (rightVolume / 8.f);

	return std::make_pair(static_cast<int16_t>(leftSample), static_cast<int16_t>(rightSample));
}