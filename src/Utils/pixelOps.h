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

	constexpr void setPixel(uint8_t* buffer, uint16_t width, uint8_t x, uint8_t y, color c)
	{
		const uint32_t baseInd = (y * width * 3) + (x * 3);

		buffer[baseInd] = c.R;
		buffer[baseInd + 1] = c.G;
		buffer[baseInd + 2] = c.B;
	}
	constexpr color getPixel(const uint8_t* buffer, uint16_t width, uint8_t x, uint8_t y)
	{
		const uint32_t baseInd = (y * width * 3) + (x * 3);

		return color
		{
			buffer[baseInd],
			buffer[baseInd + 1],
			buffer[baseInd + 2]
		};
	}

	constexpr void clearBuffer(uint8_t* buffer, uint16_t width, uint16_t height, color c)
	{
		if (!buffer) return;

		for (uint16_t x = 0; x < width; x++)
		{
			for (uint16_t y = 0; y < height; y++)
				setPixel(buffer, width, x, y, c);
		}
	}
}