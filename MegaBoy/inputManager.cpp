#include "inputManager.h"
#include "bitOps.h"
#include <map>

#define INT8(val) static_cast<uint8_t>(val)

const std::map<int, uint8_t> buttonKeyConfig 
{
	{57, INT8(3)}, // Space (Start)
	{28, INT8(2)}, // Enter (Select)
	{44, INT8(1)}, // Z (B)
	{45, INT8(0)}  // X (A)
};
const std::map<int, uint8_t> dpadKeyConfig 
{
	// WASD
	{31, INT8(3)},
	{17, INT8(2)},
	{30, INT8(1)},
	{32, INT8(0)},

	// Arrows
	{336, INT8(3)},
	{328, INT8(2)},
	{331, INT8(1)},
	{333, INT8(0)},
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

void inputManager::updateKeyGroup(int scancode, int action, uint8_t& keyState, const std::map<int, uint8_t>& keyConfig, bool setReg)
{
	auto key = keyConfig.find(scancode);
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

void inputManager::update(int scancode, int action)
{
	updateKeyGroup(scancode, action, dpadState, dpadKeyConfig, !getBit(joypadReg, 4));
	updateKeyGroup(scancode, action, buttonState, buttonKeyConfig, !getBit(joypadReg, 5));
}
