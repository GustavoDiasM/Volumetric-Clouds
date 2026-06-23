#pragma once
#include <glad/gl.h>

// ─────────────────────────────────────────────────────────────────────────────
//  NoiseGenerator  — gera texturas 3D de ruído via compute shaders
//
//  CONCEITO:
//  As nuvens são definidas por duas texturas 3D de ruído (técnica de Schneider
//  para Horizon: Zero Dawn, 2015):
//
//  • shapeNoise (128³ RGBA):
//      R = Perlin-Worley (forma base)  → baixa frequência
//      G = Worley octave 1
//      B = Worley octave 2
//      A = Worley octave 3
//    Combinar canais com FBM dá a forma "fofinha" das nuvens.
//
//  • detailNoise (32³ RGB):
//      R = Worley octave 1  → detalha bordas
//      G = Worley octave 2
//      B = Worley octave 3
//    Usado para erodir/detalhar a forma base.
//
//  Ambas são geradas uma vez na GPU (compute shaders) e ficam em memória.
// ─────────────────────────────────────────────────────────────────────────────
class NoiseGenerator
{
public:
    NoiseGenerator();
    ~NoiseGenerator();

    // Gera as duas texturas. Chamar uma vez no init.
    void generate();

    GLuint getShapeNoise()  const { return shapeNoiseTex; }
    GLuint getDetailNoise() const { return detailNoiseTex; }

private:
    GLuint shapeNoiseTex  = 0;
    GLuint detailNoiseTex = 0;

    GLuint shapeNoiseProgram  = 0;
    GLuint detailNoiseProgram = 0;

    GLuint createTexture3D(int size, GLenum internalFormat);
    void   compileComputeShader(GLuint& prog, const char* path);
};
