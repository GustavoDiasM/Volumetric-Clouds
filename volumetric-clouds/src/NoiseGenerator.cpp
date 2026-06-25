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

// ── Perlin clássico (tabela de permutação de Ken Perlin) ─────────────────────
// Produz ruído muito mais suave do que hash float

static const int PERM[512] = {
    151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
    140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
    247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
     57,177, 33, 88,237,149, 56, 87,174, 20,125,136,171,168, 68,175,
     74,165, 71,134,139, 48, 27,166, 77,146,158,231, 83,111,229,122,
     60,211,133,230,220,105, 92, 41, 55, 46,245, 40,244,102,143, 54,
     65, 25, 63,161,  1,216, 80, 73,209, 76,132,187,208, 89, 18,169,
    200,196,135,130,116,188,159, 86,164,100,109,198,173,186,  3, 64,
     52,217,226,250,124,123,  5,202, 38,147,118,126,255, 82, 85,212,
    207,206, 59,227, 47, 16, 58, 17,182,189, 28, 42,223,183,170,213,
    119,248,152,  2, 44,154,163, 70,221,153,101,155,167, 43,172,  9,
    129, 22, 39,253, 19, 98,108,110, 79,113,224,232,178,185,112,104,
    218,246, 97,228,251, 34,242,193,238,210,144, 12,191,179,162,241,
     81, 51,145,235,249, 14,239,107, 49,192,214, 31,181,199,106,157,
    184, 84,204,176,115,121, 50, 45,127,  4,150,254,138,236,205, 93,
    222,114, 67, 29, 24, 72,243,141,128,195, 78, 66,215, 61,156,180,
    // repetição
    151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
    140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
    247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
     57,177, 33, 88,237,149, 56, 87,174, 20,125,136,171,168, 68,175,
     74,165, 71,134,139, 48, 27,166, 77,146,158,231, 83,111,229,122,
     60,211,133,230,220,105, 92, 41, 55, 46,245, 40,244,102,143, 54,
     65, 25, 63,161,  1,216, 80, 73,209, 76,132,187,208, 89, 18,169,
    200,196,135,130,116,188,159, 86,164,100,109,198,173,186,  3, 64,
     52,217,226,250,124,123,  5,202, 38,147,118,126,255, 82, 85,212,
    207,206, 59,227, 47, 16, 58, 17,182,189, 28, 42,223,183,170,213,
    119,248,152,  2, 44,154,163, 70,221,153,101,155,167, 43,172,  9,
    129, 22, 39,253, 19, 98,108,110, 79,113,224,232,178,185,112,104,
    218,246, 97,228,251, 34,242,193,238,210,144, 12,191,179,162,241,
     81, 51,145,235,249, 14,239,107, 49,192,214, 31,181,199,106,157,
    184, 84,204,176,115,121, 50, 45,127,  4,150,254,138,236,205, 93,
    222,114, 67, 29, 24, 72,243,141,128,195, 78, 66,215, 61,156,180
};

static float fade(float t) { return t*t*t*(t*(t*6.0f-15.0f)+10.0f); }
static float lerp(float a, float b, float t) { return a+(b-a)*t; }
static float clamp01(float x) { return x<0.0f?0.0f:x>1.0f?1.0f:x; }

static float gradPerlin(int hash, float x, float y, float z)
{
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h==12||h==14) ? x : z;
    return ((h&1)?-u:u) + ((h&2)?-v:v);
}

static float perlin(float x, float y, float z)
{
    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;
    int Z = (int)floorf(z) & 255;
    x -= floorf(x); y -= floorf(y); z -= floorf(z);
    float u=fade(x), v=fade(y), w=fade(z);
    int A=PERM[X]+Y,   AA=PERM[A]+Z,   AB=PERM[A+1]+Z;
    int B=PERM[X+1]+Y, BA=PERM[B]+Z,   BB=PERM[B+1]+Z;
    return lerp(
        lerp(lerp(gradPerlin(PERM[AA],  x,  y,  z), gradPerlin(PERM[BA],  x-1,y,  z), u),
             lerp(gradPerlin(PERM[AB],  x,  y-1,z), gradPerlin(PERM[BB],  x-1,y-1,z), u), v),
        lerp(lerp(gradPerlin(PERM[AA+1],x,  y,  z-1),gradPerlin(PERM[BA+1],x-1,y,  z-1),u),
             lerp(gradPerlin(PERM[AB+1],x,  y-1,z-1),gradPerlin(PERM[BB+1],x-1,y-1,z-1),u),v),w
    ) * 0.5f + 0.5f;
}

static float perlinFBM(float x, float y, float z)
{
    return perlin(x*4,y*4,z*4)*0.500f
         + perlin(x*8,y*8,z*8)*0.350f
         + perlin(x*16,y*16,z*16)*0.150f;
}

// ── Worley com hash inteiro (muito melhor distribuição das células) ────────────

static float hashCell(int x, int y, int z, int seed)
{
    unsigned int h = (unsigned)(x*73856093 ^ y*19349663 ^ z*83492791 ^ seed*12345678);
    h ^= h>>16; h *= 0x45d9f3bu; h ^= h>>16;
    return float(h & 0xFFFFFF) / float(0x1000000);
}

static float worley(float px, float py, float pz, int freq)
{
    px *= freq; py *= freq; pz *= freq;
    int ix = (int)floorf(px), iy = (int)floorf(py), iz = (int)floorf(pz);
    float fdx = px-ix, fdy = py-iy, fdz = pz-iz;
    float minD = 10.0f;
    for (int dz=-1; dz<=1; dz++)
    for (int dy=-1; dy<=1; dy++)
    for (int dx=-1; dx<=1; dx++) {
        int cx = ((ix+dx)%freq+freq)%freq;
        int cy = ((iy+dy)%freq+freq)%freq;
        int cz = ((iz+dz)%freq+freq)%freq;
        float fpx = (float)dx + hashCell(cx,cy,cz,0);
        float fpy = (float)dy + hashCell(cx,cy,cz,1);
        float fpz = (float)dz + hashCell(cx,cy,cz,2);
        float d = sqrtf((fdx-fpx)*(fdx-fpx)+(fdy-fpy)*(fdy-fpy)+(fdz-fpz)*(fdz-fpz));
        if (d < minD) minD = d;
    }
    return std::min(minD, 1.0f);
}

// Perlin-Worley: estrutura de nuvem "cauliflower"
static float perlinWorley(float x, float y, float z)
{
    float pw = perlinFBM(x, y, z);
    float wn = worley(x, y, z, 4);
    // Remap Perlin para amplificar onde Worley é baixo (bordas das células)
    float combined = pw * 0.6f + (1.0f - wn) * 0.4f;
    return clamp01(combined);
}

// ── Upload ────────────────────────────────────────────────────────────────────

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

// ── Geração ───────────────────────────────────────────────────────────────────

void NoiseGenerator::generate()
{
    std::cout << "[NoiseGenerator] A gerar shape noise na CPU (128³)...\n";
    {
        const int N = 128;
        std::vector<float> data(N*N*N*4);
        for (int z=0; z<N; z++)
        for (int y=0; y<N; y++)
        for (int x=0; x<N; x++) {
            float u=(x+0.5f)/N, v=(y+0.5f)/N, w=(z+0.5f)/N;
            float pw  = perlinWorley(u,v,w);
            float wn1 = 1.0f - worley(u,v,w, 4);
            float wn2 = 1.0f - worley(u,v,w, 8);
            float wn3 = 1.0f - worley(u,v,w,16);
            int idx = (z*N*N + y*N + x)*4;
            data[idx+0] = clamp01(pw);
            data[idx+1] = clamp01(wn1);
            data[idx+2] = clamp01(wn2);
            data[idx+3] = clamp01(wn3);
        }
        shapeNoiseTex = uploadTexture3D(N, data);
    }

    std::cout << "[NoiseGenerator] A gerar detail noise na CPU (32³)...\n";
    {
        const int N = 32;
        std::vector<float> data(N*N*N*4);
        for (int z=0; z<N; z++)
        for (int y=0; y<N; y++)
        for (int x=0; x<N; x++) {
            float u=(x+0.5f)/N, v=(y+0.5f)/N, w=(z+0.5f)/N;
            float wn1 = 1.0f - worley(u,v,w, 2);
            float wn2 = 1.0f - worley(u,v,w, 4);
            float wn3 = 1.0f - worley(u,v,w, 8);
            int idx = (z*N*N + y*N + x)*4;
            data[idx+0] = clamp01(wn1);
            data[idx+1] = clamp01(wn2);
            data[idx+2] = clamp01(wn3);
            data[idx+3] = 1.0f;
        }
        detailNoiseTex = uploadTexture3D(N, data);
    }

    std::cout << "[NoiseGenerator] Texturas prontas.\n";
}
