#pragma once
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <array>
#include <cstring>

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

		static constexpr color fromRGB5(uint16_t rgb5, bool colorCorrection)
		{
			uint8_t R = rgb5 & 0x1F;
			uint8_t G = (rgb5 >> 5) & 0x1F;
			uint8_t B = (rgb5 >> 10) & 0x1F;

			if (colorCorrection)
			{
				constexpr std::array<uint8_t, 32> gammaTable
				{
					0, 62, 64, 89, 90, 109, 111, 127, 128, 142, 143, 156, 156, 168, 169, 180, 181,
					191, 192, 201, 202, 211, 212, 221, 221, 230, 230, 238, 239, 247, 247, 255
				};

				const uint8_t Rx = (13 * R + 2 * G + B) >> 4;
				const uint8_t Gx = (3 * G + B) >> 2;
				const uint8_t Bx = (2 * G + 14 * B) >> 4;

				R = gammaTable[Rx];
				G = gammaTable[Gx];
				B = gammaTable[Bx];
			}
			else
			{
				constexpr auto toRGB8 = [](uint8_t rgb5) -> uint8_t
				{
					return (rgb5 << 3) | (rgb5 >> 2);
				};

				R = toRGB8(R);
				G = toRGB8(G);
				B = toRGB8(B);
			}

			return color { R, G, B };
		}

		static inline color fromHex(std::string hexStr)
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

		inline std::string toHex() const 
		{
			std::stringstream ss;
			ss << "#" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
								  << static_cast<int>(R) << std::setw(2) << static_cast<int>(G) << std::setw(2) << static_cast<int>(B);
			return ss.str();
		}
	};

	inline void setPixel(uint8_t* buffer, int width, int x, int y, color c)
	{
		auto* pixel { reinterpret_cast<color*>(buffer + (y * width + x) * 3) };
		*pixel = { c.R, c.G, c.B };
	}
	inline color getPixel(const uint8_t* buffer, int width, int x, int y)
	{
		auto* pixel { reinterpret_cast<const color*>(buffer + (y * width + x) * 3) };
		return *pixel;
	}

	inline void clearBuffer(uint8_t* buffer, int width, int height, color c) 
	{
		if (!buffer)
			return;

		const size_t totalBytes = width * height * 3;

		if (c.R == c.G && c.G == c.B)
		{
			std::memset(buffer, c.R, totalBytes);
			return;
		}

		constexpr size_t PATTERN_SIZE = 384;
		std::array<uint8_t, PATTERN_SIZE> pattern;

		for (int i = 0; i < PATTERN_SIZE; i += 3)
		{
			pattern[i] = c.R;
			pattern[i + 1] = c.G;
			pattern[i + 2] = c.B;
		}

		size_t i = 0;

		for (; i + PATTERN_SIZE <= totalBytes; i += PATTERN_SIZE)
			std::memcpy(buffer + i, pattern.data(), PATTERN_SIZE);

		for (; i + 3 <= totalBytes; i += 3)
		{
			buffer[i] = c.R;
			buffer[i + 1] = c.G;
			buffer[i + 2] = c.B;
		}
	}
}