#define MINIAUDIO_IMPLEMENTATION
#include <MiniAudio/miniaudio.h>

#include "APU.h"
#include "../GBCore.h"
#include <iostream>
#include "../Utils/bitOps.h"

extern GBCore gbCore;

void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	APU* apu = (APU*)pDevice->pUserData;

	if (gbCore.emulationPaused || !gbCore.cartridge.ROMLoaded || !apu->enableAudio)
		return;

	int16_t* pOutput16 = (int16_t*)pOutput;

	for (uint32_t i = 0; i < frameCount; i++)
	{
		for (int i = 0; i < APU::CYCLES_PER_SAMPLE; i++)
			apu->execute();

		//if (apu->readIndex == apu->writeIndex)
		//{
		//	pOutput16[i * 2] = 0;
		//	pOutput16[i * 2 + 1] = 0;
		//}
		//else
		{
			//auto sample = apu->sampleBuffer[apu->readIndex];

			const auto sample = apu->sample * apu->volume;
			pOutput16[i * 2] = sample;
			pOutput16[i * 2 + 1] = sample;
			//apu->readIndex = (apu->readIndex + 1) % APU::BUFFER_SIZE;
		}
	}

	if (apu->recording)
	{
		size_t bufferLen = apu->recordingBuffer.size();
		size_t newBufferLen = bufferLen + (frameCount * APU::CHANNELS);

		apu->recordingBuffer.resize(newBufferLen);
		std::memcpy(&apu->recordingBuffer[bufferLen], pOutput16, sizeof(int16_t) * APU::CHANNELS * frameCount);

		if (newBufferLen >= APU::SAMPLE_RATE)
		{
			apu->recordingStream.write(reinterpret_cast<char*>(&apu->recordingBuffer[0]), newBufferLen * sizeof(int16_t));
			apu->recordingBuffer.clear();
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

	recordingStream.seekp(0, std::ios::end);
	const uint32_t fileSize = static_cast<uint32_t>(recordingStream.tellp()) - 8;

	recordingStream.seekp(4, std::ios::beg);
	WRITE(fileSize);

	const uint32_t dataSize = fileSize - 36; // Header is 44 bytes, 44 - 8 = 36.
	recordingStream.seekp(40, std::ios::beg);
	WRITE(dataSize);

	recordingStream.close();
	recordingBuffer.clear();
	recordingBuffer.shrink_to_fit();
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
	deviceConfig.pUserData = this;

	ma_device_init(NULL, &deviceConfig, soundDevice.get());
	ma_device_start(soundDevice.get());
}

APU::APU()
{
	initMiniAudio();
}

APU::~APU()
{
	ma_device_uninit(soundDevice.get());
	if (recording) stopRecording();
}

void APU::resetRegs()
{
	regs = {}; // to remove

	channel1.reset();

	channel1.regs.NRx1 = 0xBF;
	channel1.regs.NRx2 = 0xF3;
	channel1.regs.NRx3 = 0xFF;
	channel1.regs.NRx4 = 0xBF;

	channel2.reset();

	channel2.regs.NRx1 = 0x3F;
	channel2.regs.NRx2 = 0xF3;
	channel2.regs.NRx3 = 0xFF;
	channel2.regs.NRx4 = 0xBF;
}



//void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
//{
//	APU* apu = (APU*)pDevice->pUserData;
//
//	int16_t* pOutput16 = (int16_t*)pOutput;
//
//	for (int i = 0; i < frameCount; i++)
//	{
//		pOutput16[i * 2] = samples[i % BUFFER_SIZE];
//		pOutput16[i * 2 + 1] = samples[i % BUFFER_SIZE];
//	}
//
//
//	//uint16_t period2 = apu->regs.NR23;
//	//period2 |= ((apu->regs.NR24 & 0x07) << 8);
//	//const uint32_t freq2 = 131072 / (2048 - period2);
//
//	//const double amplitude2 = (apu->regs.NR22 >> 4) / 15.0;
//	//static double t = 0;
//
//	//int16_t* pOutput16 = (int16_t*)pOutput;
//
//	//for (ma_uint32 i = 0; i < frameCount; i++)
//	//{
//	//	double time = t / APU::SAMPLE_RATE;
//
//	//	double sample2 = sin(2 * MA_PI * freq2 * time) > 0 ? amplitude2 : -amplitude2;
//	//	sample2 *= 32767;
//
//	//	pOutput16[i * 2] = sample2;
//	//	pOutput16[i * 2 + 1] = sample2;
//	//	t++;
//	//}
//
//
//
//	//uint16_t period1 = apu->regs.NR13;
//	//period1 |= ((apu->regs.NR14 & 0x07) << 8);
//	//const uint32_t freq1 = 131072 / (2048 - period1);
//
//
//	//uint16_t period2 = apu->regs.NR23;
//	//period2 |= ((apu->regs.NR24 & 0x07) << 8);
//	//const uint32_t freq2 = 131072 / (2048 - period2);
//
//	//const double amplitude1 = (apu->regs.NR12 >> 4) / 15.0;
//	//const double amplitude2 = (apu->regs.NR22 >> 4) / 15.0;
//
//	//static double t = 0;
//	//int16_t* pOutput16 = (int16_t*)pOutput;
//
//	//for (ma_uint32 i = 0; i < frameCount; i++)
//	//{
//	//	double time = t / APU::SAMPLE_RATE;
//
//	//	double sample1 = sin(2 * MA_PI * freq1 * time) > 0 ? amplitude1 : -amplitude1;
//	//	sample1 *= 32767;
//
//	//	double sample2 = sin(2 * MA_PI * freq2 * time) > 0 ? amplitude2 : -amplitude2;
//	//	sample2 *= 32767;
//
//	//	int16_t sample = static_cast<int16_t>(sample1 + sample2);
//
//	//	pOutput16[i * 2] = sample;
//	//	pOutput16[i * 2 + 1] = sample;
//	//	t++;
//	//}
//
//
//
//
//	//int16_t* pOutput16 = (int16_t*)pOutput;
//	//static double t = 0;
//	//constexpr double frequency = 500; // wave frequency in Hz
//	//constexpr double sampleRate = APU::SAMPLE_RATE; // Sample rate in Hz
//	//constexpr double amplitude = 1.0; // Amplitude of the wave
//
//	//for (ma_uint32 i = 0; i < frameCount; i++)
//	//{
//	//	double time = t / sampleRate;
//
//	//	//	SQUARE
//	//	double sample = sin(2 * MA_PI * frequency * time) > 0 ? amplitude : -amplitude;
//
//	//	//  TRIANGLE
//	//	//	double phase = fmod(frequency * time, 1.0);
//	//	//	double sample = amplitude * (4.0 * fabs(phase - 0.5) - 1.0);
//
//	//	//  SAWTOOTH
//	//	//	double phase = fmod(frequency * time, 1.0);
//	//	//	double sample = amplitude * (2.0 * phase - 1.0);
//
//	//	sample = (int16_t)(sample * 32767);
//
//	//	pOutput16[i * 2] = sample;
//	//	pOutput16[i * 2 + 1] = sample;
//	//	t++;
//	//}
//}

//ma_device soundDevice;

void APU::execute()
{
	executeFrameSequencer();
	channel1.executeDuty();
	channel2.executeDuty();

	//if (++sampleCycles >= CYCLES_PER_SAMPLE)
	{
		//uint8_t enabledChannels { 0 };
		float newSample { 0 };

		//if (enableChannel1 && channel1.s.triggered)
		//{
		//	enabledChannels++;
		//	newSample += channel1.getSample();
		//}
		//if (enableChannel2 && channel2.s.triggered)
		//{
		//	enabledChannels++;
		//	newSample += channel2.getSample();
		//}

		newSample += channel1.getSample() * enableChannel1;
		newSample += channel2.getSample() * enableChannel2;

		sample = (newSample / 4) * 32767;
	//	sampleCycles = 0;

		//sampleBuffer[writeIndex] = sample;
		//writeIndex = (writeIndex + 1) % BUFFER_SIZE;
	}
}

void APU::executeFrameSequencer()
{
	if (++frameSequencerCycles >= 2048)
	{
		frameSequencerCycles = 0;

		if (frameSequencerStep % 2 == 0)
		{
			channel1.executeLength();
			channel2.executeLength();

			if (frameSequencerStep == 2 || frameSequencerStep == 6)
				executeSweep();
		}
		else if (frameSequencerStep == 7)
		{
			channel1.executeEnvelope();
			channel2.executeEnvelope();
		}

		frameSequencerStep = (frameSequencerStep + 1) % 8;
	}
}

uint16_t APU::calculateSweepPeriod()
{
	uint8_t sweepStep = regs.NR10 & 0x7;
	uint16_t newFreq = shadowSweepPeriod >> sweepStep;

	if (getBit(regs.NR10, 3))
		newFreq = shadowSweepPeriod - newFreq;
	else
		newFreq = shadowSweepPeriod + newFreq;

	if (newFreq > 2047)
		channel1.s.triggered = false;

	return newFreq;
}

void APU::executeSweep()
{
	if (sweepTimer > 0)
		sweepTimer--;

	if (sweepTimer == 0)
	{
		if (sweepPace == 0) sweepTimer = 8;
		else sweepTimer = sweepPace;

		if (sweepEnabled && sweepPace > 0)
		{
			const uint16_t newPeriod = calculateSweepPeriod();
			const uint8_t sweepStep = regs.NR10 & 0x7;

			if (newPeriod < 2048 && sweepStep > 0)
			{
				//channel1.s.period = newPeriod;
				channel1.regs.NRx3 = newPeriod & 0xFF;
				channel1.regs.NRx4 |= ((newPeriod & 0x700) >> 8);

				shadowSweepPeriod = newPeriod;
				sweepPace = (regs.NR10 >> 4) & 0x7;

				calculateSweepPeriod();
			}
		}
	}
}

//void APU::executeChannel2Length()
//{
//	if (!getBit(6, regs.NR24))
//		return;
//
//	channel2LengthTimer--;
//
//	if (channel2LengthTimer = 0)
//		channel2Triggered = false;
//}
//
//void APU::executeChannel2Envelope()
//{
//	if ((regs.NR22 & 0x07) == 0) return;
//
//	if (channel2PeriodTimer > 0)
//		channel2PeriodTimer--;
//
//	if (channel2PeriodTimer == 0)
//	{
//		channel2PeriodTimer = (regs.NR22 & 0x07);
//
//		if (getBit(regs.NR22, 3))
//		{
//			if (channel2Amplitude < 0xF)
//				channel2Amplitude++;
//		}
//		else
//		{
//			if (channel2Amplitude > 0)
//				channel2Amplitude--;
//		}
//	}
//}


//void APU::execute()
//{
////	return; // TEMP
//
//	//play = false;
//	cycleCounter++;
//	square1CYcleCounter++;
//
//	uint16_t period1 = regs.NR13;
//	period1 |= ((regs.NR14 & 0x07) << 8);
//	const uint32_t freq1 = 131072 / (2048 - period1);
//
//	uint16_t period2 = regs.NR23;
//	period2 |= ((regs.NR24 & 0x07) << 8);
//	const uint32_t freq2 = 131072 / (2048 - period2);
//
//	const double amplitude1 = (regs.NR12 >> 4) / 15.0;
//	const double amplitude2 = (regs.NR22 >> 4) / 15.0;
//
//	uint8_t dutyType1 = regs.NR11 >> 6;
//	uint8_t dutyType2 = regs.NR21 >> 6;
//
//	if (square1CYcleCounter >= CPU_FREQUENCY / (freq1 * 8)) 
//	{
//		dutyStep1 = (dutyStep1 + 1) % 8;
//		square1CYcleCounter = 0;
//	}
//
//	if (square2CYcleCounter >= CPU_FREQUENCY / (freq2 * 8))
//	{
//		dutyStep2 = (dutyStep2 + 1) % 8;
//		square2CYcleCounter = 0;
//	}
//
//	if (DutyCycles[dutyType2][dutyStep2])
//	{
//		//sample1 = static_cast<int16_t>(amplitude2 * 32767); 
//	}
//
//
//	if (cycleCounter >= cyclesPerSample)
//	{
//		sample2 = (DutyCycles[dutyType2][dutyStep2] * amplitude2) * 32767;
//
//		sample = sample2;
//		cycleCounter = 0;
//		play = true;
//	}
//
//	//	// ... update state of APU ...
//
//	//	uint16_t period1 = regs.NR13;
//	//	period1 |= ((regs.NR14 & 0x07) << 8);
//	//	const uint32_t freq1 = 131072 / (2048 - period1);
//
//	//	uint16_t period2 = regs.NR23;
//	//	period2 |= ((regs.NR24 & 0x07) << 8);
//	//	const uint32_t freq2 = 131072 / (2048 - period2);
//
//	//	const double amplitude1 = (regs.NR12 >> 4) / 15.0;
//	//	const double amplitude2 = (regs.NR22 >> 4) / 15.0;
//
//	//	double time = t / SAMPLE_RATE;
//
//	//	double sample1 = sin(2 * MA_PI * freq1 * time) > 0 ? amplitude1 : -amplitude1;
//	//	sample1 *= 32767;
//
//	//	double sample2 = sin(2 * MA_PI * freq2 * time) > 0 ? amplitude2 : -amplitude2;
//	//	sample2 *= 32767;
//
//	//	int16_t sample = sample1;//static_cast<int16_t>(sample1 + sample2);
//
//	//	// Store the sample in the buffer
//	//	buffer.push_back(sample);
//
//	//	t++;
//	//}
//}