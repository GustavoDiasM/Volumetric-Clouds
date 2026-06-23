#include "CloudRenderer.h"
#include "Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>

static const float kQuadVerts[] = {
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
    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO); }
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

    // Detectar movimento da câmara para ajustar blend temporal
    float posDelta = glm::length(cam.position - prevCamPos);
    float rotDelta = glm::length(cam.front    - prevCamFront);
    prevCamPos   = cam.position;
    prevCamFront = cam.front;

    // Qualquer rotação ou translação significativa → blend alto (descarta história)
    // Em repouso → blend baixo (acumula qualidade ao longo de ~20 frames)
    float motion = posDelta * 0.0005f + rotDelta * 500.0f;
    float temporalBlend = glm::clamp(0.06f + motion, 0.06f, 0.98f);

    int curr = currentBuf;
    int prev = 1 - currentBuf;

    // ── PASS 1: ray march (resolução completa) ────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, cloudFBO[curr]);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    cloudShader->use();
    cloudShader->setMat4 ("uInvView",       invView);
    cloudShader->setMat4 ("uInvProj",       invProj);
    cloudShader->setVec3 ("uCameraPos",     cam.position);
    cloudShader->setFloat("uTime",          time);
    cloudShader->setVec2 ("uResolution",    glm::vec2(width, height));
    cloudShader->setInt  ("uFrameIndex",    frameIndex);
    cloudShader->setFloat("uTemporalBlend", temporalBlend);

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

    // Shape + detail noise
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, shapeNoiseTex);
    cloudShader->setInt("uShapeNoise", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, detailNoiseTex);
    cloudShader->setInt("uDetailNoise", 1);

    // Frame anterior (ping-pong)
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, cloudColorTex[prev]);
    cloudShader->setInt("uPrevCloudColor", 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, cloudAlphaTex[prev]);
    cloudShader->setInt("uPrevCloudAlpha", 3);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ── PASS 2: composite + tonemapping ──────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    compositeShader->use();
    compositeShader->setVec3 ("uSkyZenith",    params.skyColorZenith);
    compositeShader->setVec3 ("uSkyHorizon",   params.skyColorHorizon);
    compositeShader->setVec3 ("uFogColor",     params.fogColor);
    compositeShader->setVec3 ("uSunDir",       params.sunDirection);
    compositeShader->setVec3 ("uSunColor",     params.sunColor);
    compositeShader->setFloat("uSunIntensity", params.sunIntensity);
    compositeShader->setMat4 ("uInvView",      invView);
    compositeShader->setMat4 ("uInvProj",      invProj);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cloudColorTex[curr]);
    compositeShader->setInt("uCloudColor", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, cloudAlphaTex[curr]);
    compositeShader->setInt("uCloudAlpha", 1);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    currentBuf = 1 - currentBuf;
    frameIndex++;
}

void CloudRenderer::resize(int width, int height)
{
    destroyFramebuffer();
    createFramebuffer(width, height);
    frameIndex = 0;
}

void CloudRenderer::createQuad()
{
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

void CloudRenderer::createFramebuffer(int width, int height)
{
    for (int i = 0; i < 2; i++) {
        glGenTextures(1, &cloudColorTex[i]);
        glBindTexture(GL_TEXTURE_2D, cloudColorTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &cloudAlphaTex[i]);
        glBindTexture(GL_TEXTURE_2D, cloudAlphaTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &cloudFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, cloudFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cloudColorTex[i], 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, cloudAlphaTex[i], 0);
        GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, attachments);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "[CloudRenderer] Framebuffer " << i << " incompleto!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CloudRenderer::destroyFramebuffer()
{
    for (int i = 0; i < 2; i++) {
        if (cloudFBO[i])      { glDeleteFramebuffers(1, &cloudFBO[i]);      cloudFBO[i] = 0; }
        if (cloudColorTex[i]) { glDeleteTextures(1,      &cloudColorTex[i]); cloudColorTex[i] = 0; }
        if (cloudAlphaTex[i]) { glDeleteTextures(1,      &cloudAlphaTex[i]); cloudAlphaTex[i] = 0; }
    }
}
