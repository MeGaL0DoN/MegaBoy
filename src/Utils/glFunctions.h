#pragma once
#include <cstdint>

namespace OpenGL
{
	void createQuad(uint32_t& VAO, uint32_t& VBO, uint32_t& EBO);
	void createTexture(uint32_t& textureId, uint32_t width, uint32_t height, const uint8_t* data = nullptr, bool bilinearFilter = false);
	void bindTexture(uint32_t textureId);
	void updateTexture(uint32_t textureId, uint32_t width, uint32_t height, const uint8_t* data);
	void setTextureScalingMode(uint32_t textureId, bool bilinear);
}