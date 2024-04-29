#include "glFunctions.h"
#include <glad/glad.h>

void OpenGL::createTexture(uint32_t& textureId, uint32_t width, uint32_t height)
{
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void OpenGL::updateTexture(uint32_t textureId, uint32_t width, uint32_t height, const uint8_t* data)
{
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
}
void OpenGL::bindTexture(uint32_t textureId)
{
    glBindTexture(GL_TEXTURE_2D, textureId);
}