// src/backend/opengl/GlShader.cpp
#include "GlShader.h"
#include <cstdio>
#include <cstdlib>

namespace ege {

GlShader::GlShader()
    : m_vertexShader(0), m_fragmentShader(0), m_program(0) {
}

GlShader::~GlShader() {
    if (m_vertexShader)   glDeleteShader(m_vertexShader);
    if (m_fragmentShader) glDeleteShader(m_fragmentShader);
    if (m_program)        glDeleteProgram(m_program);
}

static bool checkShader(GLuint shader, const char* type) {
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char* log = (char*)malloc(len + 1);
        glGetShaderInfoLog(shader, len, nullptr, log);
        log[len] = '\0';
        fprintf(stderr, "GlShader: %s compile error: %s\n", type, log);
        free(log);
        return false;
    }
    return true;
}

bool GlShader::compileVertex(const char* source) {
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(m_vertexShader, 1, &source, nullptr);
    glCompileShader(m_vertexShader);
    return checkShader(m_vertexShader, "vertex");
}

bool GlShader::compileFragment(const char* source) {
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(m_fragmentShader, 1, &source, nullptr);
    glCompileShader(m_fragmentShader);
    return checkShader(m_fragmentShader, "fragment");
}

static bool checkProgram(GLuint program) {
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        char* log = (char*)malloc(len + 1);
        glGetProgramInfoLog(program, len, nullptr, log);
        log[len] = '\0';
        fprintf(stderr, "GlShader: link error: %s\n", log);
        free(log);
        return false;
    }
    return true;
}

bool GlShader::link() {
    m_program = glCreateProgram();
    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);
    glLinkProgram(m_program);
    return checkProgram(m_program);
}

void GlShader::use() const {
    glUseProgram(m_program);
}

void GlShader::setUniform1i(const char* name, int v) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniform1i(loc, v);
}

void GlShader::setUniform1f(const char* name, float v) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniform1f(loc, v);
}

void GlShader::setUniform2f(const char* name, float x, float y) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniform2f(loc, x, y);
}

void GlShader::setUniform3f(const char* name, float x, float y, float z) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniform3f(loc, x, y, z);
}

void GlShader::setUniform4f(const char* name, float x, float y, float z, float w) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}

void GlShader::setUniformMatrix3(const char* name, const float* mat) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniformMatrix3fv(loc, 1, GL_FALSE, mat);
}

void GlShader::setUniformMatrix4(const char* name, const float* mat) const {
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
}

} // namespace ege
