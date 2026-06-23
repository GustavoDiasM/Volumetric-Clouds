#include "NoiseGenerator.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

NoiseGenerator::NoiseGenerator() = default;

NoiseGenerator::~NoiseGenerator()
{
    if (shapeNoiseTex)  glDeleteTextures(1, &shapeNoiseTex);
    if (detailNoiseTex) glDeleteTextures(1, &detailNoiseTex);
}

// ── Funções de ruído na CPU (espelho dos compute shaders) ─────────────────────

static float hashf(float x, float y, float z)
{
    // Hash deterministico baseado nas constantes do shader GLSL
    float px = x * 443.8975f, py = y * 397.2973f, pz = z * 491.1871f;
    // fract
    px -= std::floor(px); py -= std::floor(py); pz -= std::floor(pz);
    float dot = px * pz + py * px + pz * py + 19.19f * (px + py + pz);
    float d = px + dot; d -= std::floor(d);
    float e = py + dot; e -= std::floor(e);
    float f = pz + dot; f -= std::floor(f);
    float r = d * e * f * 27.0f;
    r -= std::floor(r);
    return r;
}

static float fract(float x) { return x - std::floor(x); }
static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

static float worleyCPU(float px, float py, float pz, float freq)
{
    px *= freq; py *= freq; pz *= freq;
    float idx = std::floor(px), idy = std::floor(py), idz = std::floor(pz);
    float fdx = fract(px), fdy = fract(py), fdz = fract(pz);

    float minDist = 1.0f;
    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    for (int z = -1; z <= 1; z++)
    {
        float nx = idx + x, ny = idy + y, nz = idz + z;
        // mod(id + neighbor, freq)
        float cx = fract(nx / freq) * freq;
        float cy = fract(ny / freq) * freq;
        float cz = fract(nz / freq) * freq;

        float px2 = (float)x + hashf(cx, cy, cz);
        float py2 = (float)y + hashf(cx + 17.3f, cy, cz);
        float pz2 = (float)z + hashf(cx, cy + 31.7f, cz);

        float dx = fdx - px2, dy = fdy - py2, dz = fdz - pz2;
        float d = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (d < minDist) minDist = d;
    }
    return minDist;
}

static float gradDot(float ix, float iy, float iz, float fx, float fy, float fz)
{
    float h = fract(std::sin(ix * 127.1f + iy * 311.7f + iz * 74.7f) * 43758.5453f);
    float a = h * 6.2832f, b = h * 3.1416f;
    float gx = std::cos(a) * std::sin(b);
    float gy = std::sin(a) * std::sin(b);
    float gz = std::cos(b);
    return gx * fx + gy * fy + gz * fz;
}

static float perlinCPU(float px, float py, float pz)
{
    float ix = std::floor(px), iy = std::floor(py), iz = std::floor(pz);
    float fx = fract(px), fy = fract(py), fz = fract(pz);
    // Smoothstep C2
    auto smooth = [](float t){ return t*t*t*(t*(t*6.0f-15.0f)+10.0f); };
    float ux = smooth(fx), uy = smooth(fy), uz = smooth(fz);

    float v000 = gradDot(ix,   iy,   iz,   fx,   fy,   fz  );
    float v100 = gradDot(ix+1, iy,   iz,   fx-1, fy,   fz  );
    float v010 = gradDot(ix,   iy+1, iz,   fx,   fy-1, fz  );
    float v110 = gradDot(ix+1, iy+1, iz,   fx-1, fy-1, fz  );
    float v001 = gradDot(ix,   iy,   iz+1, fx,   fy,   fz-1);
    float v101 = gradDot(ix+1, iy,   iz+1, fx-1, fy,   fz-1);
    float v011 = gradDot(ix,   iy+1, iz+1, fx,   fy-1, fz-1);
    float v111 = gradDot(ix+1, iy+1, iz+1, fx-1, fy-1, fz-1);

    auto lerp = [](float a, float b, float t){ return a + (b-a)*t; };
    float r = lerp(lerp(lerp(v000,v100,ux), lerp(v010,v110,ux), uy),
                   lerp(lerp(v001,v101,ux), lerp(v011,v111,ux), uy), uz);
    return r * 0.5f + 0.5f;
}

static float perlinWorleyCPU(float px, float py, float pz, float freq)
{
    float pw = perlinCPU(px*freq*0.5f, py*freq*0.5f, pz*freq*0.5f) * 0.50f
             + perlinCPU(px*freq,      py*freq,      pz*freq)       * 0.35f
             + perlinCPU(px*freq*2.0f, py*freq*2.0f, pz*freq*2.0f) * 0.15f;
    float wn = worleyCPU(px, py, pz, freq);
    return pw * 0.5f + (1.0f - wn) * 0.5f;
}

// ── Upload para textura 3D ────────────────────────────────────────────────────

static GLuint uploadTexture3D(int size, const std::vector<float>& data)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, size, size, size,
                 0, GL_RGBA, GL_FLOAT, data.data());
    glGenerateMipmap(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, 0);
    return tex;
}

// ── Geração principal ─────────────────────────────────────────────────────────

void NoiseGenerator::generate()
{
    // ── Shape noise: 128³ ─────────────────────────────────────────────────
    std::cout << "[NoiseGenerator] A gerar shape noise na CPU (128³)...\n";
    {
        const int N = 128;
        std::vector<float> data(N * N * N * 4);
        for (int z = 0; z < N; z++)
        for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
        {
            float u = (x + 0.5f) / N;
            float v = (y + 0.5f) / N;
            float w = (z + 0.5f) / N;

            float pw = perlinWorleyCPU(u, v, w, 4.0f);
            float wn1 = 1.0f - worleyCPU(u, v, w, 4.0f);
            float wn2 = 1.0f - worleyCPU(u, v, w, 8.0f);
            float wn3 = 1.0f - worleyCPU(u, v, w, 16.0f);

            int idx = (z * N * N + y * N + x) * 4;
            data[idx+0] = clamp01(pw);
            data[idx+1] = clamp01(wn1);
            data[idx+2] = clamp01(wn2);
            data[idx+3] = clamp01(wn3);
        }
        shapeNoiseTex = uploadTexture3D(N, data);
    }

    // ── Detail noise: 32³ ─────────────────────────────────────────────────
    std::cout << "[NoiseGenerator] A gerar detail noise na CPU (32³)...\n";
    {
        const int N = 32;
        std::vector<float> data(N * N * N * 4);
        for (int z = 0; z < N; z++)
        for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
        {
            float u = (x + 0.5f) / N;
            float v = (y + 0.5f) / N;
            float w = (z + 0.5f) / N;

            float wn1 = 1.0f - worleyCPU(u, v, w, 2.0f);
            float wn2 = 1.0f - worleyCPU(u, v, w, 4.0f);
            float wn3 = 1.0f - worleyCPU(u, v, w, 8.0f);

            int idx = (z * N * N + y * N + x) * 4;
            data[idx+0] = clamp01(wn1);
            data[idx+1] = clamp01(wn2);
            data[idx+2] = clamp01(wn3);
            data[idx+3] = 1.0f;
        }
        detailNoiseTex = uploadTexture3D(N, data);
    }

    std::cout << "[NoiseGenerator] Texturas prontas.\n";
}
