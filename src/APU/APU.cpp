#define MINIAUDIO_IMPLEMENTATION
#include <MiniAudio/miniaudio.h>

#include <cstring>
#include "APU.h"
#include "../GBCore.h"
#include "../Utils/bitOps.h"
 
APU::APU(GBCore& gbCore) : gbCore(gbCore)
{
	initMiniAudio();
}

APU::~APU()
{
	ma_device_uninit(soundDevice.get());
	if (recording) stopRecording();
}

void APU::reset()
{
	channel1.reset();
	channel2.reset();

	channel1.NR10 = 0x80;
	channel1.NRx1 = 0xBF;
	channel1.NRx2 = 0xF3;
	channel1.NRx3 = 0xFF;
	channel1.NRx4 = 0xBF;

	channel2.NRx1 = 0x3F;
	channel2.NRx2 = 0xF3;
	channel2.NRx3 = 0xFF;
	channel2.NRx4 = 0xBF;
}

void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	GBCore* gb = static_cast<GBCore*>(pDevice->pUserData);
	APU& apu = gb->apu;

	if (gb->emulationPaused || !gb->cartridge.ROMLoaded() || !appConfig::enableAudio)
		return;

	int16_t* pOutput16 = (int16_t*)pOutput;

	for (uint32_t i = 0; i < frameCount; i++)
	{
		for (int i = 0; i < APU::CYCLES_PER_SAMPLE; i++)
			apu.execute();

		apu.generateSample();

		pOutput16[i * 2] = apu.getSample();
		pOutput16[i * 2 + 1] = apu.getSample();
	}

	if (apu.recording)
	{
		size_t bufferLen = apu.recordingBuffer.size();
		size_t newBufferLen = bufferLen + (frameCount * APU::CHANNELS);

		apu.recordingBuffer.resize(newBufferLen);
		std::memcpy(&apu.recordingBuffer[bufferLen], pOutput16, sizeof(int16_t) * APU::CHANNELS * frameCount);

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
	recordingStream.write("RIFF", 4);

	uint32_t lengthReserve{};
	WRITE(lengthReserve);
	recordingStream.write("WAVE", 4);
	recordingStream.write("fmt ", 4);

	constexpr uint32_t SECTION_CHUNK_SIZE = 16;
	WRITE(SECTION_CHUNK_SIZE);

	constexpr uint16_t PCM_FORMAT = 1;
	WRITE(PCM_FORMAT);

	WRITE(APU::CHANNELS);
	WRITE(APU::SAMPLE_RATE);

	constexpr uint32_t BYTE_RATE = (SAMPLE_RATE * sizeof(int16_t) * CHANNELS) / 8;
	WRITE(BYTE_RATE);

	constexpr uint16_t BLOCK_ALIGN = (BITS_PER_SAMPLE * CHANNELS) / 8;
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
	deviceConfig.playback.format =  ma_format_s16;
	deviceConfig.playback.channels =  CHANNELS;
	deviceConfig.sampleRate = SAMPLE_RATE;
	deviceConfig.dataCallback = sound_data_callback;
	deviceConfig.pUserData = &gbCore;

	ma_device_init(NULL, &deviceConfig, soundDevice.get());
	ma_device_start(soundDevice.get());
}

void APU::executeFrameSequencer()
{
	frameSequencerCycles++;

	if (frameSequencerCycles == 2048)
	{
		if (frameSequencerStep % 2 == 0)
		{
			channel1.executeLength();
			channel2.executeLength();

			if (frameSequencerStep == 2 || frameSequencerStep == 6)
				channel1.executeSweep();
		}
		else if (frameSequencerStep == 7)
		{
			channel1.executeEnvelope();
			channel2.executeEnvelope();
		}

		frameSequencerCycles = 0;
		frameSequencerStep = (frameSequencerStep + 1) % 8;
	}
}

void APU::execute()
{
	channel1.execute();
	channel2.execute();

	executeFrameSequencer();
}

void APU::generateSample()
{
	sample = 0;

	sample += channel1.getSample() * enableChannel1;
	sample += channel2.getSample() * enableChannel2;

	sample = sample / 4 * volume;
}