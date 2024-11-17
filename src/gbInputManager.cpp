#include "gbInputManager.h"
#include "Utils/bitOps.h"
#include "keyBindManager.h"
#include "CPU/CPU.h"

void gbInputManager::reset()
{
	readButtons = true;
	readDpad = true;
	dpadState = 0xF;
	buttonState = 0xF;
}

void gbInputManager::update(int key, int action)
{
	auto updateInput = [&](uint8_t& keyState, bool readGroup, int keyConfigOffset) -> bool
	{
		for (int i = keyConfigOffset; i < 4 + keyConfigOffset; i++)
		{
			if (key == KeyBindManager::keyBinds[i])
			{
				if (action)
				{
					keyState = resetBit(keyState, i - keyConfigOffset);

					if (readGroup)
						cpu.requestInterrupt(Interrupt::Joypad);
				}
				else
					keyState = setBit(keyState, i - keyConfigOffset);

				return true;
			}
		}

		return false;
	};

	if (updateInput(dpadState, readDpad, 4)) return;
	updateInput(buttonState, readButtons, 0);
}

uint8_t gbInputManager::readJoypadReg() const
{
	if (!readButtons && !readDpad)
		return 0xCF;

	uint8_t keyState{ 0xF0 };

	if (readDpad)
	{
		keyState = resetBit(keyState, 4);
		keyState |= dpadState;
	}
	if (readButtons)
	{
		keyState = resetBit(keyState, 5);
		keyState |= buttonState;
	}

	return keyState;
}

void gbInputManager::setJoypadReg(uint8_t val)
{
	readButtons = !getBit(val, 5);
	readDpad = !getBit(val, 4);
}