#pragma once
#include <cstdint>
#include <random>

namespace RngOps
{
	inline std::random_device rd;
	inline std::mt19937 gen(rd()); 
	inline std::uniform_int_distribution<std::mt19937::result_type> dist(0, 255);

	inline uint8_t gen8bit()
	{
		return static_cast<uint8_t>(dist(gen));
	}
}