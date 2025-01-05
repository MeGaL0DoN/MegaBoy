#define MINIAUDIO_IMPLEMENTATION
#include <MiniAudio/miniaudio.h>

#include <cstring>
#include "APU.h"
#include "../GBCore.h"
#include "../Utils/bitOps.h"
 
APU::APU(GBCore& gbCore) : gb(gbCore)
{ }

APU::~APU()
{
	if (soundDevice != nullptr)
		ma_device_uninit(soundDevice.get());

	if (recording)
		stopRecording();
}

void APU::reset()
{
	if (soundDevice == nullptr)
		initMiniAudio();

	channel1.reset();
	channel2.reset();
	channel3.reset();
	channel4.reset();

	regs.NR50 = 0x77;
	regs.NR51 = 0xF3;
	regs.apuEnable = true;

	frameSequencerCycles = 0;
	frameSequencerStep = 0;
}

void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	auto* gb = static_cast<GBCore*>(pDevice->pUserData);
	auto& apu = gb->apu;
	auto* pOutput16 = static_cast<int16_t*>(pOutput);

	const bool emulationStopped = gb->emulationPaused || !gb->cartridge.ROMLoaded();

	if (!appConfig::enableAudio || emulationStopped || !apu.enabled())
	{
		if (!emulationStopped && apu.enabled())
			apu.execute(APU::CYCLES_PER_SAMPLE * frameCount);

		std::memset(pOutput16, 0, sizeof(int16_t) * frameCount * APU::CHANNELS);
		return;
	}

	for (uint32_t i = 0; i < frameCount; i++)
	{
		apu.execute(APU::CYCLES_PER_SAMPLE);
		int16_t sample = apu.generateSample();

		pOutput16[i * 2] = sample;
		pOutput16[i * 2 + 1] = sample;
	}

	if (apu.recording)
	{
		size_t bufferLen = apu.recordingBuffer.size();
		size_t newBufferLen = bufferLen + (frameCount * APU::CHANNELS);

		apu.recordingBuffer.resize(newBufferLen);
		std::memcpy(&apu.recordingBuffer[bufferLen], pOutput16, sizeof(int16_t) * frameCount * APU::CHANNELS);

		if (newBufferLen >= APU::SAMPLE_RATE)
		{		
			apu.recordingStream.write(reinterpret_cast<char*>(apu.recordingBuffer.data()), newBufferLen * sizeof(int16_t));
			apu.recordingBuffer.clear();
		}
	}
}

void APU::startRecording(const std::filesystem::path& filePath)
{
	recording = true;
	recordingStream = std::ofstream(filePath);
	recordingBuffer.reserve(APU::SAMPLE_RATE);
	writeWAVHeader();
}

#define WRITE(val) recordingStream.write(reinterpret_cast<const char*>(&val), sizeof(val));

void APU::writeWAVHeader()
{
	constexpr uint16_t BITS_PER_SAMPLE = sizeof(int16_t) * CHAR_BIT;
	constexpr uint32_t BYTE_RATE = (SAMPLE_RATE * sizeof(int16_t) * CHANNELS) / 8;
	constexpr uint32_t SECTION_CHUNK_SIZE = 16;
	constexpr uint16_t BLOCK_ALIGN = (BITS_PER_SAMPLE * CHANNELS) / 8;
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
	recording = false;

	if (!recordingBuffer.empty())
		recordingStream.write(reinterpret_cast<char*>(&recordingBuffer[0]), recordingBuffer.size() * sizeof(int16_t));

	recordingStream.seekp(0, std::ios::end);
	const uint32_t fileSize = static_cast<uint32_t>(recordingStream.tellp()) - 8;

	recordingStream.seekp(4, std::ios::beg);
	WRITE(fileSize);

	const uint32_t dataSize = fileSize - 36; // Header is 44 bytes, 44 - 8 = 36.
	recordingStream.seekp(40, std::ios::beg);
	WRITE(dataSize);

	recordingStream.close();
	recordingBuffer.clear();
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
			// TODO ADD CHANNEL 4.

			if (frameSequencerStep == 2 || frameSequencerStep == 6)
				channel1.executeSweep();
		}
		else if (frameSequencerStep == 7)
		{
			channel1.executeEnvelope();
			channel2.executeEnvelope();
			// TODO ADD CHANNEL 4.
		}

		frameSequencerCycles = 0;
		frameSequencerStep = (frameSequencerStep + 1) & 7;
	}
}

void APU::execute(uint32_t cycles)
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

int16_t APU::generateSample()
{
	sample = 0;

	sample += channel1.getSample() * enabledChannels[0];
	sample += channel2.getSample() * enabledChannels[1];
	sample += channel3.getSample() * enabledChannels[2];
	sample += channel4.getSample() * enabledChannels[3];

	sample /= 4;

	return (sample * volume) * INT16_MAX;
}