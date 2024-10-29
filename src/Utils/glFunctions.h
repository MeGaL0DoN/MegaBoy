#pragma once
#include <cstdint>

namespace OpenGL
{
	void bindTexture(uint32_t textureId);
	void createTexture(uint32_t& textureId, uint32_t width, uint32_t height, const uint8_t* data = nullptr, bool bilinearFilter = false);
	void updateTexture(uint32_t textureId, uint32_t width, uint32_t height, const uint8_t* data);
	void setTextureScalingMode(uint32_t textureId, bool bilinear);
}