#pragma once
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace PixelOps
{
	struct color
	{
	public:
		uint8_t R, G, B;

		bool operator ==(color other) const
		{
			return R == other.R && G == other.G && B == other.B;
		}

		constexpr static color fromRGB5(uint16_t rgb5)
		{
			return color
			{
				componentFromRGB5(rgb5 & 0x1F),
				componentFromRGB5((rgb5 >> 5) & 0x1F),
				componentFromRGB5((rgb5 >> 10) & 0x1F)
			};
		}

		static color fromHex(std::string hexStr)
		{
			if (hexStr[0] == '#')
				hexStr = hexStr.substr(1);

			uint32_t hexVal;
			std::stringstream ss;
			ss << std::hex << hexStr;
			ss >> hexVal;

			return color
			{
				static_cast<uint8_t>((hexVal >> 16) & 0xFF),
				static_cast<uint8_t>((hexVal >> 8) & 0xFF),
				static_cast<uint8_t>(hexVal & 0xFF)
			};
		}

		std::string toHex() const 
		{
			std::stringstream ss;
			ss << "#" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
								  << static_cast<int>(R) << std::setw(2) << static_cast<int>(G) << std::setw(2) << static_cast<int>(B);
			return ss.str();
		}
	private:
		constexpr static uint8_t componentFromRGB5(uint8_t rgb5)
		{
			return (rgb5 << 3) | (rgb5 >> 2);
		}
	};

	inline void setPixel(uint8_t* buffer, uint16_t width, uint8_t x, uint8_t y, color c)
	{
		auto pixel = reinterpret_cast<color*>(buffer + (y * width + x) * 3);
		*pixel = { c.R, c.G, c.B };
	}
	inline color getPixel(const uint8_t* buffer, uint16_t width, uint8_t x, uint8_t y)
	{
		const auto pixel = reinterpret_cast<const color*>(buffer + (y * width + x) * 3);
		return *pixel;
	}

	inline void clearBuffer(uint8_t* buffer, uint16_t width, uint16_t height, color c) 
	{
		if (!buffer) return;

		const size_t totalBytes = width * height * 3;

		if (c.R == c.G && c.G == c.B)
		{
			std::memset(buffer, c.R, totalBytes);
			return;
		}

		constexpr size_t PATTERN_SIZE = 384;
		uint8_t pattern[PATTERN_SIZE];

		for (int i = 0; i < PATTERN_SIZE; i += 3)
		{
			pattern[i] = c.R;
			pattern[i + 1] = c.G;
			pattern[i + 2] = c.B;
		}

		size_t i = 0;

		for (; i + PATTERN_SIZE <= totalBytes; i += PATTERN_SIZE)
			std::memcpy(buffer + i, pattern, PATTERN_SIZE);

		for (; i + 3 <= totalBytes; i += 3)
		{
			buffer[i] = c.R;
			buffer[i + 1] = c.G;
			buffer[i + 2] = c.B;
		}
	}
}