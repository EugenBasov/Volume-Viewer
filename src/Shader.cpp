#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace {

std::string readTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        std::cerr << "Cannot open " << path << std::endl;
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint compileShader(GLenum type, const std::string& path)
{
    const std::string source = readTextFile(path);
    const char* code = source.c_str();

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Failed to compile " << path << ":\n" << log << std::endl;
    }
    return shader;
}

} // namespace

GLuint loadShaderProgram(const std::string& vertexPath, const std::string& fragmentPath)
{
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexPath);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentPath);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Failed to link the shader program:\n" << log << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
