#define MINIAUDIO_IMPLEMENTATION
#include <MiniAudio/miniaudio.h>

#include "APU.h"
#include "GBCore.h"
#include <iostream>

extern GBCore gbCore;

void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	if (gbCore.options.paused)
		return;

	APU* apu = (APU*)pDevice->pUserData;

	//int16_t* pOutput16 = (int16_t*)pOutput;
	//static double t = 0;
	//constexpr double frequency = 500; // wave frequency in Hz
	//constexpr double sampleRate = APU::SAMPLE_RATE; // Sample rate in Hz
	//constexpr double amplitude = 1.0; // Amplitude of the wave

	//for (ma_uint32 i = 0; i < frameCount; i++)
	//{
	//	double time = t / sampleRate;

	//	//	SQUARE
	//	double sample = sin(2 * MA_PI * frequency * time) > 0 ? amplitude : -amplitude;

	//	//  TRIANGLE
	//	//	double phase = fmod(frequency * time, 1.0);
	//	//	double sample = amplitude * (4.0 * fabs(phase - 0.5) - 1.0);

	//	//  SAWTOOTH
	//	//	double phase = fmod(frequency * time, 1.0);
	//	//	double sample = amplitude * (2.0 * phase - 1.0);

	//	sample = (int16_t)(sample * 32767);

	//	pOutput16[i * 2] = sample;
	//	pOutput16[i * 2 + 1] = sample;
	//	t++;
	//}
}

ma_device soundDevice;

void APU::initMiniAudio()
{
	ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16; // ma_format_u8
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate = SAMPLE_RATE;
	deviceConfig.dataCallback = sound_data_callback;

	deviceConfig.pUserData = this;

	ma_device_init(NULL, &deviceConfig, &soundDevice);
	ma_device_start(&soundDevice);
}

void APU::execute()
{

}