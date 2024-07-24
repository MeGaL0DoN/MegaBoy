#pragma once
#include <cstdint>

namespace PixelOps
{
	struct color
	{
	public:
		uint8_t R, G, B;

		bool operator ==(color other)
		{
			return R == other.R && G == other.G && B == other.B;
		}

		constexpr static color fromRGB5(uint16_t rgb5)
		{
			color c;

			c.R = componentFromRGB5(rgb5 & 0x1F);
			c.G = componentFromRGB5((rgb5 >> 5) & 0x1F);
			c.B = componentFromRGB5((rgb5 >> 10) & 0x1F);

			return c;
		}
	private:
		constexpr static uint8_t componentFromRGB5(uint8_t rgb5)
		{
			return (rgb5 << 3) | (rgb5 >> 2);
		}
	};

	constexpr void setPixel(uint8_t* buffer, uint16_t width, uint8_t x, uint8_t y, color c)
	{
		uint32_t baseInd = (y * width * 3) + (x * 3);

		buffer[baseInd] = c.R;
		buffer[baseInd + 1] = c.G;
		buffer[baseInd + 2] = c.B;
	}
	constexpr color getPixel(const uint8_t* buffer, uint16_t width, uint8_t x, uint8_t y)
	{
		uint32_t baseInd = (y * width * 3) + (x * 3);

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

		for (uint8_t x = 0; x < width; x++)
		{
			for (uint8_t y = 0; y < height; y++)
				setPixel(buffer, width, x, y, c);
		}
	}
}