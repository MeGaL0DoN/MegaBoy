#pragma once
#include <cstdint>

template<typename T>
constexpr uint8_t getBit(T val, uint8_t bit) { return static_cast<bool>(val & (1 << bit)); }

template<typename T>
constexpr T resetBit(T val, uint8_t bit) { return val & ~(1 << bit); }

template<typename T>
constexpr T setBit(T val, uint8_t bit) { return val | (1 << bit); }

template<typename T>
constexpr T setBit(T num, uint8_t bit, bool val)
{
	if (val) return setBit<T>(num, bit);
	return resetBit<T>(num, bit);
}