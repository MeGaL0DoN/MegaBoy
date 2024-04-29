#pragma once
#include "cstdint"

class APU
{
public:
	friend class MMU;

	void reset();
private:

	// Square 1
	uint8_t NR10;
	uint8_t NR11;
	uint8_t NR12;
	uint8_t NR13;
	uint8_t NR14;

	// Square 2
	uint8_t NR21;
	uint8_t NR22;
	uint8_t NR23;
	uint8_t NR24;

	// Wave
	uint8_t NR30;
	uint8_t NR31;
	uint8_t NR32;
	uint8_t NR33;
	uint8_t NR34;

	// Noise
	uint8_t NR41;
	uint8_t NR42;
	uint8_t NR43;
	uint8_t NR44;

	// Control/Status
	uint8_t NR50;
	uint8_t NR51;
	uint8_t NR52;

};