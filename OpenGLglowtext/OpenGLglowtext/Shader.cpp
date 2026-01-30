#include "Shader.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    std::string vertexSrc = ReadFile(vertexPath);
    std::string fragmentSrc = ReadFile(fragmentPath);
    m_RendererID = CreateProgram(vertexSrc, fragmentSrc);
}

Shader::~Shader()
{
    glDeleteProgram(m_RendererID);
}

void Shader::Bind() const
{
    glUseProgram(m_RendererID);
}

void Shader::Unbind() const
{
    glUseProgram(0);
}

std::string Shader::ReadFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[Shader] Failed to open file: " << path << std::endl;
        return "";
    }

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}


unsigned int Shader::CompileShader(unsigned int type, const std::string& source)
{
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << info << std::endl;
    }
    return shader;
}

unsigned int Shader::CreateProgram(const std::string& vs, const std::string& fs)
{
    unsigned int program = glCreateProgram();
    unsigned int vert = CompileShader(GL_VERTEX_SHADER, vs);
    unsigned int frag = CompileShader(GL_FRAGMENT_SHADER, fs);

    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glDeleteShader(vert);
    glDeleteShader(frag);


    int linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char info[1024];
        glGetProgramInfoLog(program, 1024, nullptr, info);
        std::cerr << "[Shader LINK ERROR]\n" << info << std::endl;
    }


    return program;
}


int Shader::GetLocation(const std::string& name) const
{
    int loc = glGetUniformLocation(m_RendererID, name.c_str());
    if (loc == -1)
        std::cerr << "[Shader] Uniform not found: " << name << std::endl;
    return loc;
}

void Shader::SetInt(const std::string& name, int value) const
{
    glUniform1i(GetLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) const
{
    glUniform1f(GetLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) const
{
    glUniform2f(GetLocation(name), value.x, value.y); // GLint location, GLfloat v0, GLfloat v1
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3f(GetLocation(name), value.x, value.y, value.z); // GLint location, GLfloat v0, GLfloat v1, GLfloat v2
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) const
{
    glUniform4f(GetLocation(name), value.x, value.y, value.z, value.w); // GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3
}

void Shader::SetMat4(const std::string& name, const glm::mat4& matrix) const
{
    glUniformMatrix4fv(
        GetLocation(name),
        1,
        GL_FALSE,
        glm::value_ptr(matrix)
    );
}