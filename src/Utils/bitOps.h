#pragma once
#include <cstdint>
#include <array>

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

namespace hexOps
{
    constexpr std::array<char, 16> LOWER_CASE_HEX_CHARS = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    constexpr std::array<char, 16> UPPER_CASE_HEX_CHARS = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    template <bool upperCase>
    inline std::string toHexStr(uint8_t i)
    {
        constexpr auto& hexChars = upperCase ? UPPER_CASE_HEX_CHARS : LOWER_CASE_HEX_CHARS;
        std::string hexStr(2, '0');

        hexStr[0] = hexChars[(i & 0xF0) >> 4];
        hexStr[1] = hexChars[i & 0x0F];

        return hexStr;
    }

    template <bool upperCase>
    inline std::string toHexStr(uint16_t i)
    {
        constexpr auto& hexChars = upperCase ? UPPER_CASE_HEX_CHARS : LOWER_CASE_HEX_CHARS;
        std::string hexStr(4, '0');

        hexStr[0] = hexChars[(i & 0xF000) >> 12];
        hexStr[1] = hexChars[(i & 0x0F00) >> 8];
        hexStr[2] = hexChars[(i & 0x00F0) >> 4];
        hexStr[3] = hexChars[i & 0x000F];

        return hexStr;
    }

    inline uint8_t fromHex(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';

        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');

        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');

        return 0;
    }

    inline uint8_t fromHex(std::array<char, 2> hexChars)
    {
        return (fromHex(hexChars[0]) << 4) | fromHex(hexChars[1]);
    }

    inline uint16_t fromHex(std::array<char, 4> hexChars)
    {
        return (fromHex(hexChars[0]) << 12) |
               (fromHex(hexChars[1]) << 8)  |
               (fromHex(hexChars[2]) << 4)  |
                fromHex(hexChars[3]);
    }
}