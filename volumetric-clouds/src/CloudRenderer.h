#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <memory>
#include "Camera.h"

class Shader;
class NoiseGenerator;

// ─────────────────────────────────────────────────────────────────────────────
//  CloudParams  — todos os parâmetros ajustáveis da nuvem
//  Expostos ao ImGui para tweaking em tempo real.
// ─────────────────────────────────────────────────────────────────────────────
struct CloudParams
{
    // Camada de nuvens (em metros/unidades de mundo)
    float cloudBottomAltitude = 1500.0f;
    float cloudTopAltitude    = 4000.0f;

    // Forma
    float cloudCoverage      = 0.45f;  // [0,1] — quanto do céu está coberto
    float cloudDensity       = 0.3f;   // multiplicador de densidade
    float shapeNoiseScale    = 0.00008f;
    float detailNoiseScale   = 0.0004f;
    float detailNoiseStrength = 0.25f; // quanto o detail erode a shape

    // Vento
    glm::vec3 windDirection  = glm::vec3(1.0f, 0.0f, 0.1f);
    float windSpeed          = 50.0f;

    // Lighting
    glm::vec3 sunDirection   = glm::normalize(glm::vec3(0.5f, 0.8f, 0.2f));
    glm::vec3 sunColor       = glm::vec3(1.0f, 0.9f, 0.7f);
    float     sunIntensity   = 1.0f;
    float     lightAbsorption = 0.75f;  // absorção ao longo do raio de luz
    float     cloudAbsorption = 0.5f;   // absorção ao longo do raio de câmara
    float     darkEdgeFactor  = 0.4f;   // escurece o topo das nuvens (sombra própria)

    // Scattering — Henyey-Greenstein
    float     hgForward      = 0.8f;    // lóbulo forward (sol atrás = halo brilhante)
    float     hgBackward     = -0.3f;   // lóbulo backward
    float     hgBlend        = 0.5f;    // mistura entre lóbulos

    // Ray march
    int   primarySteps   = 64;   // passos ao longo do raio de câmara
    int   lightSteps     = 6;    // passos do raio de luz (shadow march)
    float maxRayDistance = 80000.0f;

    // Sky
    glm::vec3 skyColorZenith  = glm::vec3(0.1f, 0.3f, 0.7f);
    glm::vec3 skyColorHorizon = glm::vec3(0.5f, 0.7f, 1.0f);
    glm::vec3 fogColor        = glm::vec3(0.8f, 0.85f, 0.95f);
};

// ─────────────────────────────────────────────────────────────────────────────
//  CloudRenderer  — gere o pipeline completo de renderização das nuvens
//
//  Pipeline:
//    1. Render low-res pass (render target ½ res) → cloud color + transmittance
//    2. Upscale + composite sobre o fundo do céu
//    3. Tonemapping (Reinhard ou ACES)
// ─────────────────────────────────────────────────────────────────────────────
class CloudRenderer
{
public:
    CloudParams params;

    CloudRenderer();
    ~CloudRenderer();

    void init(int width, int height, GLuint shapeNoise, GLuint detailNoise);
    void render(const Camera& cam, float time, int width, int height);
    void resize(int width, int height);

private:
    // Shaders
    std::unique_ptr<Shader> cloudShader;     // fullscreen cloud ray march
    std::unique_ptr<Shader> compositeShader; // composite clouds + tonemapping

    // Fullscreen quad
    GLuint quadVAO = 0, quadVBO = 0;

    // Low-res framebuffer para as nuvens (½ resolução)
    GLuint cloudFBO      = 0;
    GLuint cloudColorTex = 0;   // RGB = cor das nuvens acumulada
    GLuint cloudAlphaTex = 0;   // R = transmittância (0=opaco, 1=transparente)

    // Noise textures (owned by NoiseGenerator)
    GLuint shapeNoiseTex  = 0;
    GLuint detailNoiseTex = 0;

    void createQuad();
    void createFramebuffer(int width, int height);
    void destroyFramebuffer();
};
