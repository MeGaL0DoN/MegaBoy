#pragma once
#include <string>

class Shader
{
public:
    unsigned int ID {};

    Shader() = default;
    Shader(const char* vertexPath, const char* fragmentPath) { compile(vertexPath, fragmentPath); };

    void compile(const char* vertexPath, const char* fragmentPath);
    void use();

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setFloat2(const std::string& name, float value1, float value2) const;
    void setFloat4(const std::string& name, float values[4]) const;

private:
    void checkCompileErrors(unsigned int shader, std::string type);
};

