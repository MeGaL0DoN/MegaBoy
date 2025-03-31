#include "Joypad.h"
#include "keyBindManager.h"
#include "CPU/CPU.h"
#include "Utils/bitOps.h"

void Joypad::reset()
{
	if (System::Current() == GBSystem::DMGCompatMode)
	{
		readButtons = false;
		readDpad = false;
	}
	else
	{
		readButtons = true;
		readDpad = true;
	}

	dpadState = 0xF;
	buttonState = 0xF;
}

void Joypad::update(int key, bool action)
{
	const auto updateInput = [&](uint8_t& keyState, bool readGroup, int keyConfigOffset) -> bool
	{
		for (int i = keyConfigOffset; i < 4 + keyConfigOffset; i++)
		{
			if (key == KeyBindManager::keyBinds[i])
			{
				keyState = setBit(keyState, i - keyConfigOffset, !action);

				if (action && readGroup)
					cpu.requestInterrupt(Interrupt::Joypad);

				return true;
			}
		}

		return false;
	};

	if (updateInput(dpadState, readDpad, 4)) return;
	updateInput(buttonState, readButtons, 0);
}

uint8_t Joypad::readInputReg() const
{
	if (!readButtons && !readDpad)
		return 0xFF;

	uint8_t keyState { 0xF0 };

	if (readDpad)
	{
		keyState = resetBit(keyState, 4);
		keyState |= dpadState;

		// Don't allow opposite directions to read as pressed at the same time as its impossible on the gameboy and can break some games.
		// Instead, reaturn both as not pressed.

		if (!getBit(keyState, 0) && !getBit(keyState, 1))
			keyState |= 0b11;

		if (!getBit(keyState, 2) && !getBit(keyState, 3))
			keyState |= 0b1100;
	}
	if (readButtons)
	{
		keyState = resetBit(keyState, 5);
		keyState |= buttonState;
	}

	return keyState;
}

void Joypad::writeInputReg(uint8_t val)
{
	readButtons = !getBit(val, 5);
	readDpad = !getBit(val, 4);
}