#pragma once
#include <cstdint>

constexpr bool getBit(uint8_t val, uint8_t bit) { return val & (1 << bit); }
constexpr uint8_t resetBit(uint8_t val, uint8_t bit) { return val & ~(1 << bit); }
constexpr uint8_t setBit(uint8_t val, uint8_t bit) { return val | (1 << bit); }

constexpr uint8_t setBit(uint8_t num, uint8_t bit, bool val)
{
	if (val) return setBit(num, bit);
	return resetBit(num, bit);
}