#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  Shader  — carrega, compila e linka programas GLSL
//
//  Uso:
//    Shader s("shaders/cloud.vert", "shaders/cloud.frag");
//    s.use();
//    s.setFloat("time", t);
//
//  Para compute shaders:
//    Shader cs("shaders/noise.comp");
//    cs.use();
//    glDispatchCompute(x, y, z);
// ─────────────────────────────────────────────────────────────────────────────
class Shader
{
public:
    GLuint id = 0;

    // Vertex + Fragment
    Shader(const std::string& vertPath, const std::string& fragPath);
    // Compute only
    explicit Shader(const std::string& compPath);

    ~Shader();

    void use() const { glUseProgram(id); }

    // Helpers de uniform ─────────────────────────────────────────────────────
    void setBool (const std::string& name, bool  v) const;
    void setInt  (const std::string& name, int   v) const;
    void setFloat(const std::string& name, float v) const;
    void setVec2 (const std::string& name, const glm::vec2& v) const;
    void setVec3 (const std::string& name, const glm::vec3& v) const;
    void setVec4 (const std::string& name, const glm::vec4& v) const;
    void setMat4 (const std::string& name, const glm::mat4& v) const;

private:
    static std::string  readFile(const std::string& path);
    static GLuint       compileShader(GLenum type, const std::string& src);
    static void         checkErrors(GLuint obj, bool isProgram);
};
