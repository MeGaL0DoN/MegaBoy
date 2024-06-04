#define MINIAUDIO_IMPLEMENTATION
#include <MiniAudio/miniaudio.h>

#include "APU.h"
#include "GBCore.h"
#include <iostream>

extern GBCore gbCore;

void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	if (gbCore.emulationPaused)
		return;

	APU* apu = (APU*)pDevice->pUserData;
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
			pOutput16[i * 2] = apu->sample;
			pOutput16[i * 2 + 1] = apu->sample;
			//apu->readIndex = (apu->readIndex + 1) % APU::BUFFER_SIZE;
		}
	}
}

void APU::initMiniAudio()
{
	soundDevice = std::make_unique<ma_device>();

	ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = 2;
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
	uint16_t period2 = regs.NR23;
	period2 |= ((regs.NR24 & 0x07) << 8);
	const uint32_t freq2 = 131072 / (2048 - period2);

	const double amplitude2 = (regs.NR22 >> 4) / 15.0;
	uint8_t dutyType2 = regs.NR21 >> 6;

	if (++channel2Cycles >= CPU_FREQUENCY / (freq2 * 8))
	{
		dutyStep2 = (dutyStep2 + 1) % 8;
		channel2Cycles = 0;
	}

	if (++cycles >= CYCLES_PER_SAMPLE)
	{
		/*int16_t*/ sample = (dutyCycles[dutyType2][dutyStep2] * amplitude2) * 32767;
		cycles = 0;

		//sampleBuffer[writeIndex] = sample;
		//writeIndex = (writeIndex + 1) % BUFFER_SIZE;
	}
}


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