#version 430 core

// ─────────────────────────────────────────────────────────────────────────────
//  composite.frag — faz o composite das nuvens sobre o céu + tonemapping
//
//  Pipeline:
//    1. Gera a cor de fundo (céu procedural + sol)
//    2. Compõe as nuvens sobre o fundo usando transmittância
//    3. Aplica tonemapping ACES para converter HDR → LDR
//    4. Correcção gamma (linear → sRGB)
// ─────────────────────────────────────────────────────────────────────────────

in vec2 vUV;
out vec4 oFragColor;

uniform sampler2D uCloudColor;     // cor das nuvens (½ res)
uniform sampler2D uCloudAlpha;     // transmittância (½ res)

uniform vec3  uSkyZenith;
uniform vec3  uSkyHorizon;
uniform vec3  uFogColor;
uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform float uSunIntensity;

// Para reconstruir a direcção do raio (céu procedural)
uniform mat4 uInvView;
uniform mat4 uInvProj;

// ── Tonemapping ACES (Academy Color Encoding System) ─────────────────────────
//  Curva S que preserva detalhes em highlights e shadows.
//  Referência: Krzysztof Narkowicz, "ACES Filmic Tone Mapping Curve", 2016.
vec3 tonemapACES(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ── Cor do céu (Rayleigh scattering simplificado) ────────────────────────────
vec3 skyColor(vec3 rd)
{
    // Gradiente zenith→horizonte
    float t = clamp(pow(max(rd.y, 0.0), 0.4), 0.0, 1.0);
    vec3 sky = mix(uSkyHorizon, uSkyZenith, t);

    // Neblina no horizonte
    float fog = pow(1.0 - max(rd.y, 0.0), 6.0);
    sky = mix(sky, uFogColor, fog * 0.6);

    return sky;
}

// ── Disco solar ───────────────────────────────────────────────────────────────
vec3 sunDisk(vec3 rd)
{
    float cosTheta = dot(rd, normalize(uSunDir));
    // Ângulo do disco solar: ~0.5° (0.00872 rad) → cos ≈ 0.99996
    float disk = smoothstep(0.9995, 0.9999, cosTheta);
    // Halo suave à volta do sol
    float halo = pow(max(cosTheta, 0.0), 32.0) * 0.1;
    return (disk + halo) * uSunColor * uSunIntensity * 3.0;
}

void main()
{
    // ── 1. Reconstruir direcção do raio ───────────────────────────────────
    vec2 ndc  = vUV * 2.0 - 1.0;
    vec4 clip = vec4(ndc, -1.0, 1.0);
    vec4 eye  = uInvProj * clip;
    eye = vec4(eye.xy, -1.0, 0.0);
    vec3 rd   = normalize((uInvView * eye).xyz);

    // ── 2. Cor de fundo (céu + sol) ───────────────────────────────────────
    vec3 background = skyColor(rd) + sunDisk(rd);

    // ── 3. Ler resultado das nuvens (½ resolução, upscale bilinear gratuito)
    vec3  clouds       = texture(uCloudColor, vUV).rgb;
    float transmit     = texture(uCloudAlpha, vUV).r;

    // Composite: cor final = cor das nuvens + fundo atenuado pela transmittância
    // transmit = 1 → pixel transparente (só fundo)
    // transmit = 0 → pixel opaco (só nuvem)
    vec3 color = clouds + background * transmit;

    // ── 4. Tonemapping + correcção gamma ─────────────────────────────────
    color = tonemapACES(color);
    color = pow(color, vec3(1.0 / 2.2)); // linear → sRGB

    oFragColor = vec4(color, 1.0);
}
