// src/backend/opengl/GlShader.h
// Shader compilation and management utility
#pragma once
#include "glad/gl.h"

namespace ege {

class GlShader {
public:
    GlShader();
    ~GlShader();

    bool compileVertex(const char* source);
    bool compileFragment(const char* source);
    bool link();

    void use() const;

    GLuint getProgram() const { return m_program; }

    // Uniform setters
    void setUniform1i(const char* name, int v) const;
    void setUniform1f(const char* name, float v) const;
    void setUniform2f(const char* name, float x, float y) const;
    void setUniform3f(const char* name, float x, float y, float z) const;
    void setUniform4f(const char* name, float x, float y, float z, float w) const;
    void setUniformMatrix3(const char* name, const float* mat) const;
    void setUniformMatrix4(const char* name, const float* mat) const;

private:
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    GLuint m_program;
};

} // namespace ege
