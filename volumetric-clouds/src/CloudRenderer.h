#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <memory>
#include "Camera.h"

class Shader;

struct CloudParams
{
    float cloudBottomAltitude = 1500.0f;
    float cloudTopAltitude    = 4000.0f;
    float cloudCoverage       = 0.50f;
    float cloudDensity        = 0.25f;
    float shapeNoiseScale     = 0.000065f;
    float detailNoiseScale    = 0.00035f;
    float detailNoiseStrength = 0.20f;

    glm::vec3 windDirection   = glm::vec3(1.0f, 0.0f, 0.1f);
    float     windSpeed       = 40.0f;

    glm::vec3 sunDirection    = glm::normalize(glm::vec3(0.5f, 0.8f, 0.2f));
    glm::vec3 sunColor        = glm::vec3(1.0f, 0.92f, 0.75f);
    float     sunIntensity    = 2.5f;
    float     lightAbsorption = 0.40f;
    float     cloudAbsorption = 0.20f;
    float     darkEdgeFactor  = 0.60f;

    float hgForward  =  0.85f;
    float hgBackward = -0.25f;
    float hgBlend    =  0.50f;

    int   primarySteps   = 64;
    int   lightSteps     = 8;
    float maxRayDistance = 80000.0f;

    glm::vec3 skyColorZenith  = glm::vec3(0.08f, 0.25f, 0.65f);
    glm::vec3 skyColorHorizon = glm::vec3(0.55f, 0.72f, 0.95f);
    glm::vec3 fogColor        = glm::vec3(0.80f, 0.85f, 0.95f);
};

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
    std::unique_ptr<Shader> cloudShader;
    std::unique_ptr<Shader> compositeShader;

    GLuint quadVAO = 0, quadVBO = 0;

    // Ping-pong FBOs para acumulação temporal (resolução completa)
    GLuint cloudFBO[2]      = {0,0};
    GLuint cloudColorTex[2] = {0,0};
    GLuint cloudAlphaTex[2] = {0,0};
    int    currentBuf       = 0;

    GLuint shapeNoiseTex  = 0;
    GLuint detailNoiseTex = 0;

    int frameIndex = 0;
    glm::vec3 prevCamPos   = glm::vec3(0.0f);
    glm::vec3 prevCamFront = glm::vec3(0.0f, 0.0f, -1.0f);

    void createQuad();
    void createFramebuffer(int width, int height);
    void destroyFramebuffer();
};
