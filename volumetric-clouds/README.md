# Renderização Volumétrica de Nuvens em Tempo Real

**Gustavo Dias Martins** — Projeto de Computação Gráfica, Universidade da Beira Interior

Renderizador de nuvens volumétricas em tempo real implementado em OpenGL 4.2 + C++17, com base nas técnicas de Andrew Schneider (SIGGRAPH 2015, *Horizon Zero Dawn*).

---

## Técnicas Implementadas

### 1. Geração de Ruído Procedural (CPU)
Duas texturas 3D geradas na CPU durante a inicialização:

- **Shape noise** (128³ RGBA): Perlin-Worley (canal R) + Worley invertido a frequências 4, 8 e 16 (canais G, B, A). Usa a tabela de permutação clássica de Ken Perlin com FBM de 3 oitavas.
- **Detail noise** (32³ RGBA): Worley invertido a frequências 2, 4 e 8 para erosão das bordas.

### 2. Ray Marching Volumétrico
Para cada pixel, um raio atravessa a camada de nuvens (entre `cloudBottom` e `cloudTop` metros). O passo é limitado a 150 m para evitar subamostragem a ângulos rasantes. O ponto de partida é perturbado por jitter IGN (Interleaved Gradient Noise) para amostragem estocástica.

### 3. Modelo de Iluminação Física
- **Beer-Lambert**: `T = exp(-density × absorption × stepSize)` — atenuação ao longo do raio de luz
- **Henyey-Greenstein dupla**: dois lobos (g⁺ = 0.85 forward, g⁻ = −0.25 backward) para o halo de retroiluminação (*silver lining*)
- **Powder effect**: `1 − exp(−density × k × 2)` — aproximação de múltiplo espalhamento, escurece as bordas inferiores

### 4. Acumulação Temporal (TAA)
Dois pares de FBO em ping-pong. O blend adaptativo aumenta de 0.10 (câmara parada) até 0.98 (câmara em movimento) para evitar ghosting. O jitter IGN garante amostras diferentes por fotograma.

### 5. Pipeline a Dois Passes

| Passe | Shader | Saída |
|-------|--------|-------|
| 1 | `clouds.frag` | Cor das nuvens (RGBA16F) + Transmitância (R16F) em FBO |
| 2 | `composite.frag` | Céu procedural + composição + tonemapping ACES + gamma |

### 6. Céu Procedural e Tonemapping ACES
Gradiente zenith-horizonte com névoa e disco solar. Tonemapping ACES com curva S fotográfica seguida de correção gamma (γ = 2.2) para conversão HDR → sRGB.

---

## Compilar e Executar

### Pré-requisitos
- CMake ≥ 3.20
- GCC/Clang com suporte C++17
- Git (para FetchContent)
- **Linux/WSL**: `sudo apt-get install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`

### Build

```bash
chmod +x build.sh
./build.sh          # Release (recomendado)
./build.sh debug    # Debug
```

Ou manualmente:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./VolumetricClouds
```

A primeira compilação descarrega automaticamente via CMake FetchContent:
- GLFW 3.4 (windowing)
- GLM 1.0.1 (matemática — header-only)
- GLAD v2 (OpenGL loader para GL 4.2 Core)
- ImGui v1.90.9 (UI imediata)

---

## Controlos

| Input | Ação |
|-------|------|
| Botão direito (manter) | Capturar rato / olhar |
| W / A / S / D | Mover câmara |
| Space / Ctrl | Subir / Descer |
| Scroll | FOV |
| ESC | Sair |

---

## Estrutura do Projeto

```
volumetric-clouds/
├── CMakeLists.txt              — build system (FetchContent)
├── build.sh                    — script de compilação
├── src/
│   ├── main.cpp                — ponto de entrada
│   ├── Application.h/.cpp      — janela GLFW, loop, ImGui
│   ├── Camera.h/.cpp           — câmara FPS livre
│   ├── Shader.h/.cpp           — compilação e link de shaders GLSL
│   ├── CloudRenderer.h/.cpp    — pipeline de renderização (2 passes, ping-pong TAA)
│   └── NoiseGenerator.h/.cpp   — geração de texturas 3D de ruído na CPU
├── shaders/
│   ├── fullscreen.vert         — vertex shader partilhado (fullscreen quad NDC)
│   ├── clouds.frag             — ray marching, iluminação, TAA
│   └── composite.frag          — composição, céu procedural, tonemapping ACES
└── relatorio/                  — relatório LaTeX do projeto
```

---

## Referências

- A. Schneider & N. Vos, *"The Real-time Volumetric Cloudscapes of Horizon Zero Dawn"*, SIGGRAPH 2015
- L. G. Henyey & J. L. Greenstein, *"Diffuse Radiation in the Galaxy"*, The Astrophysical Journal, 1941
- K. Perlin, *"An Image Synthesizer"*, SIGGRAPH 1985
- S. Worley, *"A Cellular Texture Basis Function"*, SIGGRAPH 1996
- N. Max, *"Optical Models for Direct Volume Rendering"*, IEEE TVCG, 1995
- S. Hillaire, *"A Scalable and Production Ready Sky and Atmosphere Rendering Technique"*, EGSR 2020
- B. Karis, *"High Quality Temporal Supersampling"*, SIGGRAPH 2014
- K. Narkowicz, *"ACES Filmic Tone Mapping Curve"*, 2016
