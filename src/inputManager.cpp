#include "inputManager.h"
#include "Utils/bitOps.h"

#include <map>

#define INT8(val) static_cast<uint8_t>(val)

// GLFW to gameboy key mappings

const std::map<int, uint8_t> buttonKeyConfig 
{
	{32, INT8(3)}, // Space (Start)
	{257,INT8(2)}, // Enter (Select)
	{90, INT8(1)}, // Z (B)
	{88, INT8(0)}  // X (A)
};
const std::map<int, uint8_t> dpadKeyConfig 
{
	// WASD
	{87, INT8(2)},
	{65, INT8(1)},
	{83, INT8(3)},
	{68, INT8(0)},

	// Arrows
	{265, INT8(2)},
	{263, INT8(1)},
	{264, INT8(3)},
	{262, INT8(0)},
};

#undef INT8

void inputManager::reset()
{
	joypadReg = 0xCF;
	dpadState = 0xF;
	buttonState = 0xF;
}

void inputManager::modeChanged()
{
	const bool readButtons = !getBit(joypadReg, 5);
	const bool readDpad = !getBit(joypadReg, 4);

	if (readButtons) joypadReg = (joypadReg & 0xF0) | buttonState;
	if (readDpad) joypadReg = (joypadReg & 0xF0) | dpadState;
}

void inputManager::updateKeyGroup(int keyCode, int action, uint8_t& keyState, const std::map<int, uint8_t>& keyConfig, bool setReg)
{
	auto key = keyConfig.find(keyCode);
	if (key != keyConfig.end())
	{
		keyState = setBit(keyState, key->second, !action);
		if (setReg)
		{
			joypadReg = setBit(joypadReg, key->second, !action);
			if (action) cpu.requestInterrupt(Interrupt::Joypad);
		}
	}
}

void inputManager::update(int key, int action)
{
	updateKeyGroup(key, action, dpadState, dpadKeyConfig, !getBit(joypadReg, 4));
	updateKeyGroup(key, action, buttonState, buttonKeyConfig, !getBit(joypadReg, 5));
}
