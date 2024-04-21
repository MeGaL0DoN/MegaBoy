#pragma once
#include <cstdint>

namespace OpenGL
{
	void createTexture(uint32_t& textureId, uint32_t width, uint32_t height);
	void updateTexture(uint32_t textureId, uint32_t width, uint32_t height, const uint8_t* data);
}