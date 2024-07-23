#include "squareWave.h"
#include "APU.h"
#include "bitOps.h"

void squareWave::trigger()
{
	s.triggered = true; 
	if (s.lengthTimer == 0) updateLengthTimer(); 
	s.periodTimer = (regs.NRx2 & 0x07); 
	s.amplitude = (regs.NRx2 >> 4);
	s.frequencyTimer = 2048 - getPeriod();
}

void squareWave::executeDuty()
{
	if (--s.frequencyTimer == 0)
	{
		s.frequencyTimer = 2048 - getPeriod();
		s.dutyStep = (s.dutyStep + 1) % 8;
	}

	//const uint32_t freq = 131072 / (2048 - getPeriod());
	//const uint8_t dutyType = regs.NRx1 >> 6;

	//if (++s.dutyCycles >= APU::CPU_FREQUENCY / (freq * 8))
	//{
	//	s.dutyStep = (s.dutyStep + 1) % 8;
	//	s.dutyCycles = 0;
	//}
}

void squareWave::executeEnvelope()
{
	if ((regs.NRx2 & 0x07) == 0) return;

	if (s.periodTimer > 0)
		s.periodTimer--;

	if (s.periodTimer == 0)
	{
		s.periodTimer = (regs.NRx2 & 0x07);

		if (getBit(regs.NRx2, 3))
		{
			if (s.amplitude < 0xF)
				s.amplitude++;
		}
		else
		{
			if (s.amplitude > 0)
				s.amplitude--;
		}
	}
}

void squareWave::executeLength()
{
	if (!getBit(6, regs.NRx4))
		return;

	s.lengthTimer--;

	if (s.lengthTimer == 0)
		s.triggered = false;
}