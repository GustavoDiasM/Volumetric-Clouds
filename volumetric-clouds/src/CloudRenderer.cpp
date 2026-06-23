#include "CloudRenderer.h"
#include "Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

// ── Vértices do fullscreen quad (NDC) ──────────────────────────────────────
static const float kQuadVerts[] = {
    // pos (xy)   uv
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
};

CloudRenderer::CloudRenderer() = default;

CloudRenderer::~CloudRenderer()
{
    if (quadVAO)  { glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO); }
    destroyFramebuffer();
}

void CloudRenderer::init(int width, int height, GLuint shapeNoise, GLuint detailNoise)
{
    shapeNoiseTex  = shapeNoise;
    detailNoiseTex = detailNoise;

    createQuad();
    createFramebuffer(width, height);

    cloudShader     = std::make_unique<Shader>("shaders/fullscreen.vert", "shaders/clouds.frag");
    compositeShader = std::make_unique<Shader>("shaders/fullscreen.vert", "shaders/composite.frag");

    std::cout << "[CloudRenderer] Pronto.\n";
}

void CloudRenderer::render(const Camera& cam, float time, int width, int height)
{
    const glm::mat4 invView = glm::inverse(cam.getViewMatrix());
    const glm::mat4 invProj = glm::inverse(cam.getProjectionMatrix((float)width / height));

    // ── PASS 1: ray march das nuvens (½ resolução) ─────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, cloudFBO);
    glViewport(0, 0, width / 2, height / 2);
    glClear(GL_COLOR_BUFFER_BIT);

    cloudShader->use();
    // Matrizes inversas para reconstruir raios a partir de NDC
    cloudShader->setMat4("uInvView",      invView);
    cloudShader->setMat4("uInvProj",      invProj);
    cloudShader->setVec3("uCameraPos",    cam.position);
    cloudShader->setFloat("uTime",        time);
    cloudShader->setVec2("uResolution",   glm::vec2(width / 2, height / 2));

    // Parâmetros das nuvens
    cloudShader->setFloat("uCloudBottom",        params.cloudBottomAltitude);
    cloudShader->setFloat("uCloudTop",           params.cloudTopAltitude);
    cloudShader->setFloat("uCoverage",           params.cloudCoverage);
    cloudShader->setFloat("uDensity",            params.cloudDensity);
    cloudShader->setFloat("uShapeScale",         params.shapeNoiseScale);
    cloudShader->setFloat("uDetailScale",        params.detailNoiseScale);
    cloudShader->setFloat("uDetailStrength",     params.detailNoiseStrength);
    cloudShader->setVec3 ("uWindDir",            params.windDirection);
    cloudShader->setFloat("uWindSpeed",          params.windSpeed);
    cloudShader->setVec3 ("uSunDir",             params.sunDirection);
    cloudShader->setVec3 ("uSunColor",           params.sunColor);
    cloudShader->setFloat("uSunIntensity",       params.sunIntensity);
    cloudShader->setFloat("uLightAbsorption",    params.lightAbsorption);
    cloudShader->setFloat("uCloudAbsorption",    params.cloudAbsorption);
    cloudShader->setFloat("uDarkEdgeFactor",     params.darkEdgeFactor);
    cloudShader->setFloat("uHGForward",          params.hgForward);
    cloudShader->setFloat("uHGBackward",         params.hgBackward);
    cloudShader->setFloat("uHGBlend",            params.hgBlend);
    cloudShader->setInt  ("uPrimarySteps",       params.primarySteps);
    cloudShader->setInt  ("uLightSteps",         params.lightSteps);
    cloudShader->setFloat("uMaxRayDist",         params.maxRayDistance);
    cloudShader->setVec3 ("uSkyZenith",          params.skyColorZenith);
    cloudShader->setVec3 ("uSkyHorizon",         params.skyColorHorizon);

    // Bind noise textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, shapeNoiseTex);
    cloudShader->setInt("uShapeNoise", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, detailNoiseTex);
    cloudShader->setInt("uDetailNoise", 1);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ── PASS 2: composite + tonemapping (resolução final) ──────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    compositeShader->use();
    compositeShader->setVec3 ("uSkyZenith",  params.skyColorZenith);
    compositeShader->setVec3 ("uSkyHorizon", params.skyColorHorizon);
    compositeShader->setVec3 ("uFogColor",   params.fogColor);
    compositeShader->setVec3 ("uSunDir",     params.sunDirection);
    compositeShader->setVec3 ("uSunColor",   params.sunColor);
    compositeShader->setFloat("uSunIntensity", params.sunIntensity);
    compositeShader->setMat4 ("uInvView",    invView);
    compositeShader->setMat4 ("uInvProj",    invProj);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cloudColorTex);
    compositeShader->setInt("uCloudColor", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, cloudAlphaTex);
    compositeShader->setInt("uCloudAlpha", 1);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void CloudRenderer::resize(int width, int height)
{
    destroyFramebuffer();
    createFramebuffer(width, height);
}

// ── Privados ──────────────────────────────────────────────────────────────────

void CloudRenderer::createQuad()
{
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);

    // posição (location 0): 2 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // UV (location 1): 2 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

void CloudRenderer::createFramebuffer(int width, int height)
{
    int w = width  / 2;
    int h = height / 2;

    glGenTextures(1, &cloudColorTex);
    glBindTexture(GL_TEXTURE_2D, cloudColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &cloudAlphaTex);
    glBindTexture(GL_TEXTURE_2D, cloudAlphaTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &cloudFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, cloudFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cloudColorTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, cloudAlphaTex, 0);

    GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[CloudRenderer] Framebuffer incompleto!\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CloudRenderer::destroyFramebuffer()
{
    if (cloudFBO)      { glDeleteFramebuffers(1, &cloudFBO); cloudFBO = 0; }
    if (cloudColorTex) { glDeleteTextures(1, &cloudColorTex); cloudColorTex = 0; }
    if (cloudAlphaTex) { glDeleteTextures(1, &cloudAlphaTex); cloudAlphaTex = 0; }
}
