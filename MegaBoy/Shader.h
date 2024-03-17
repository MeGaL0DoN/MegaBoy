#pragma once
#include <string>
//#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtc/type_ptr.hpp>

class Shader
{
public:
    unsigned int ID {};

	Shader(const char* vertexPath, const char* fragmentPath);
    Shader() = default;

    void use();
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setFloat2(const std::string& name, float value1, float value2) const;
    void setFloat4(const std::string& name, float values[4]) const;
    //void setMatrix4(const std::string& name, glm::mat4) const;

private:
    void checkCompileErrors(unsigned int shader, std::string type);
};

