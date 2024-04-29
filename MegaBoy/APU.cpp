#include "APU.h"

void APU::reset() 
{
	NR10 = 0x80;
	NR11 = 0xBF;
	NR12 = 0xF3;
	NR13 = 0xFF;
	NR14 = 0xBF;
	NR21 = 0x3F;
	NR22 = 0x00;
	NR23 = 0xFF;
	NR24 = 0xBF;
	NR30 = 0x7F;
	NR31 = 0xFF;
	NR32 = 0x9F;
	NR33 = 0xFF;
	NR34 = 0xBF;
	NR41 = 0xFF;
	NR42 = 0x00;
	NR43 = 0x00;
	NR44 = 0xBF;
	NR50 = 0x77;
	NR51 = 0xF3;
	NR52 = 0xF1;
}