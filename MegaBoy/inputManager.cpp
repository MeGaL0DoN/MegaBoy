#include "inputManager.h"
#include "bitOps.h"
#include <map>

std::map<int, uint8_t> buttonKeyConfig =
{
	{57, 3}, // Space
	{28, 2}, // Enter
	{45, 1}, // X
	{44, 0}  // Z 
};
std::map<int, uint8_t> dpadKeyConfig =
{
	// WASD
	{31, 3},
	{17, 2},
	{30, 1},
	{32, 0},

	// Arrows
	{336, 3},
	{328, 2},
	{331, 1},
	{333, 0},
};

void inputManager::reset()
{
	dpadState = 0xF;
	buttonState = 0xF;

	readDpad = false;
	readButtons = false;
}

void inputManager::modeChanged(uint8_t joypadReg)
{
	readButtons = !getBit(joypadReg, 5);
	readDpad = !getBit(joypadReg, 4);

	if (readButtons) joypadReg = (joypadReg & 0xF0) | buttonState;
	if (readDpad) joypadReg = (joypadReg & 0xF0) | dpadState;

	mmu.directWrite(0xFF00, joypadReg);
}

void inputManager::updateKeyGroup(int scancode, int action, uint8_t& keyState, const std::map<int, uint8_t>& keyConfig, bool setReg, uint8_t& joypadReg)
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
	uint8_t joypadReg = mmu.directRead(0xFF00);

	updateKeyGroup(scancode, action, dpadState, dpadKeyConfig, readDpad, joypadReg);
	updateKeyGroup(scancode, action, buttonState, buttonKeyConfig, readButtons, joypadReg);

	mmu.directWrite(0xFF00, joypadReg);
}
