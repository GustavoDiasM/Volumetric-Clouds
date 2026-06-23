#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

// ── Construtores ──────────────────────────────────────────────────────────────

Shader::Shader(const std::string& vertPath, const std::string& fragPath)
{
    GLuint vert = compileShader(GL_VERTEX_SHADER,   readFile(vertPath));
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, readFile(fragPath));

    id = glCreateProgram();
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);
    checkErrors(id, true);

    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::Shader(const std::string& compPath)
{
    GLuint comp = compileShader(GL_COMPUTE_SHADER, readFile(compPath));

    id = glCreateProgram();
    glAttachShader(id, comp);
    glLinkProgram(id);
    checkErrors(id, true);

    glDeleteShader(comp);
}

Shader::~Shader()
{
    if (id) glDeleteProgram(id);
}

// ── Uniforms ──────────────────────────────────────────────────────────────────

void Shader::setBool (const std::string& n, bool  v) const { glUniform1i (glGetUniformLocation(id, n.c_str()), (int)v); }
void Shader::setInt  (const std::string& n, int   v) const { glUniform1i (glGetUniformLocation(id, n.c_str()), v); }
void Shader::setFloat(const std::string& n, float v) const { glUniform1f (glGetUniformLocation(id, n.c_str()), v); }
void Shader::setVec2 (const std::string& n, const glm::vec2& v) const { glUniform2fv(glGetUniformLocation(id, n.c_str()), 1, &v[0]); }
void Shader::setVec3 (const std::string& n, const glm::vec3& v) const { glUniform3fv(glGetUniformLocation(id, n.c_str()), 1, &v[0]); }
void Shader::setVec4 (const std::string& n, const glm::vec4& v) const { glUniform4fv(glGetUniformLocation(id, n.c_str()), 1, &v[0]); }
void Shader::setMat4 (const std::string& n, const glm::mat4& v) const { glUniformMatrix4fv(glGetUniformLocation(id, n.c_str()), 1, GL_FALSE, &v[0][0]); }

// ── Privados ──────────────────────────────────────────────────────────────────

std::string Shader::readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Shader::readFile – não foi possível abrir: " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint Shader::compileShader(GLenum type, const std::string& src)
{
    GLuint s = glCreateShader(type);
    const char* cstr = src.c_str();
    glShaderSource(s, 1, &cstr, nullptr);
    glCompileShader(s);
    checkErrors(s, false);
    return s;
}

void Shader::checkErrors(GLuint obj, bool isProgram)
{
    GLint success;
    char info[1024];
    if (isProgram) {
        glGetProgramiv(obj, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(obj, 1024, nullptr, info);
            throw std::runtime_error(std::string("Shader link error:\n") + info);
        }
    } else {
        glGetShaderiv(obj, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(obj, 1024, nullptr, info);
            throw std::runtime_error(std::string("Shader compile error:\n") + info);
        }
    }
}
