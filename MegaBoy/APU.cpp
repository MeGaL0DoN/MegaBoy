#define MINIAUDIO_IMPLEMENTATION
#include <MiniAudio/miniaudio.h>

#include "APU.h"
#include "GBCore.h"
#include <iostream>

extern GBCore gbCore;

struct soundData
{
	APU* apu;
	ma_waveform* waveForm;
};

soundData sound_data;

void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	//ma_waveform_read_pcm_frames(sound_data.waveForm, pOutput, frameCount, nullptr);

	const int32_t cpuCycles = GBCore::calculateCycles(static_cast<double>(frameCount) / APU::SAMPLE_RATE);
	gbCore.update(cpuCycles);
}

ma_device soundDevice;
ma_waveform squareWave;

void APU::initMiniAudio()
{
	constexpr int frequency = 440;

	ma_waveform_config config;
	ma_device_config deviceConfig;

	config = ma_waveform_config_init(ma_format_f32, 2, SAMPLE_RATE, ma_waveform_type_square, 1.0, frequency);
	ma_waveform_init(&config, &squareWave);

	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_f32;
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate = SAMPLE_RATE;
	deviceConfig.dataCallback = sound_data_callback;

	sound_data.waveForm = &squareWave;
	sound_data.apu = this;

	ma_device_init(NULL, &deviceConfig, &soundDevice);
	ma_device_start(&soundDevice);
}