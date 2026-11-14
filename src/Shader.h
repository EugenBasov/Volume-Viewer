#pragma once

#include <glad/glad.h>

#include <string>

// Compiles and links a shader program, returns 0 on failure
GLuint loadShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
