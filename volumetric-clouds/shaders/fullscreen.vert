#version 420 core

// ─────────────────────────────────────────────────────────────────────────────
//  Vertex shader partilhado por todos os passes de fullscreen quad.
//  Recebe posição em NDC [-1,1] e coordenadas UV [0,1].
// ─────────────────────────────────────────────────────────────────────────────

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

void main()
{
    vUV         = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
