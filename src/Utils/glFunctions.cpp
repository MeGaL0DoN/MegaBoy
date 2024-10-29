#include "glFunctions.h"

#ifdef EMSCRIPTEN
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

void OpenGL::setTextureScalingMode(uint32_t textureId, bool bilinear)
{
    bindTexture(textureId);
    const int scaleMode = bilinear ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, scaleMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, scaleMode);
}

void OpenGL::createTexture(uint32_t& textureId, uint32_t width, uint32_t height, const uint8_t* data, bool bilinearFilter)
{
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    setTextureScalingMode(textureId, bilinearFilter);
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