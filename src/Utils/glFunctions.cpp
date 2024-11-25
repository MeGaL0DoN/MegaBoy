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

void OpenGL::createQuad(uint32_t& VAO, uint32_t& VBO, uint32_t& EBO)
{
    constexpr unsigned int indices[] =
    {
        0, 1, 3,
        1, 2, 3
    };
    constexpr float vertices[] =
    {
        1.0f,  1.0f, 0.0f,  1.0f,  0.0f,  // top right     
        1.0f, -1.0f, 0.0f,  1.0f,  1.0f,  // bottom right
       -1.0f, -1.0f, 0.0f,  0.0f,  1.0f,  // bottom left
       -1.0f,  1.0f, 0.0f,  0.0f,  0.0f   // top left 
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}