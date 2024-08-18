#pragma once
#include <string>

class Shader
{
public:
    unsigned int ID {};
    constexpr bool compiled() { return ID != 0; }

    Shader() = default;
    Shader(const char* vertexCode, const char* fragmentCode) { compile(vertexCode, fragmentCode); };

    void compile(const char* vertexCode, const char* fragmentCode);
    void use();

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setFloat2(const std::string& name, float value1, float value2) const;
    void setFloat4(const std::string& name, float values[4]) const;

private:
    void checkCompileErrors(unsigned int shader, std::string type);
};

