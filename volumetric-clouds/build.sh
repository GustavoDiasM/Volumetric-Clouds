#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
#  build.sh — Compila o projeto VolumetricClouds
#  Requer: cmake >= 3.20, g++ >= 10, git, libx11-dev (Linux)
#
#  Uso:
#    chmod +x build.sh
#    ./build.sh          # compila em Release (recomendado)
#    ./build.sh debug    # compila em Debug
# ─────────────────────────────────────────────────────────────────────────────

set -e

BUILD_TYPE="Release"
if [ "$1" = "debug" ]; then
    BUILD_TYPE="Debug"
fi

echo "=============================="
echo "  Volumetric Clouds Build"
echo "  Tipo: $BUILD_TYPE"
echo "=============================="

# Instalar dependências no Linux (necessário para GLFW)
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "[Linux] A verificar dependências do sistema..."
    sudo apt-get install -y \
        libx11-dev libxrandr-dev libxinerama-dev \
        libxcursor-dev libxi-dev libgl1-mesa-dev \
        2>/dev/null || echo "  (apt falhou — assegurar que as libs X11 estão instaladas)"
fi

# Criar e entrar na pasta de build
mkdir -p build
cd build

echo "[CMake] A configurar..."
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE

echo "[Make] A compilar... (a primeira vez faz download das libs e demora)"
cmake --build . --parallel $(nproc 2>/dev/null || echo 4)

echo ""
echo "=============================="
echo "  Build concluído!"
echo "  Executável: build/VolumetricClouds"
echo "=============================="
echo ""
echo "  Controlos:"
echo "    Botão direito do rato = capturar câmara"
echo "    WASD            = mover câmara"
echo "    Space / Ctrl    = subir / descer"
echo "    Scroll          = zoom (FOV)"
echo "    ESC             = sair"
echo ""
