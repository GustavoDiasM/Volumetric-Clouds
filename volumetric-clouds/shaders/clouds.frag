#version 430 core

// ═════════════════════════════════════════════════════════════════════════════
//  clouds.frag  — Ray marching de nuvens volum╔tricas
//
//  ALGORITMO PRINCIPAL:
//
//  Para cada pixel, lança um raio da câmara para o espaço.
//  O raio intersecta a camada de nuvens (entre uCloudBottom e uCloudTop).
//  Ao longo do raio, amostra a densidade da nuvem a cada passo.
//  Para cada amostra com densidade > 0, lança um raio secundário na direcção
//  do sol para calcular a auto-sombra (light march).
//
//  FÍSICA IMPLEMENTADA:
//
//  1. Beer-Lambert Law: transmittance = exp(-density * absorption * stepSize)
//     Modela como a luz se atenua ao atravessar um meio participativo.
//
//  2. Henyey-Greenstein Phase Function:
//     p(θ) = (1 - g²) / (4π * (1 + g² - 2g*cosθ)^(3/2))
//     Modela o scattering anisotrópico: nuvens iluminadas pelo sol de trás
//     ficam com um halo brilhante (forward scattering, g > 0).
//
//  3. Powder Effect (dark edges):
//     Simula multiple scattering de forma económica — o topo das nuvens
//     é mais escuro porque a luz tem de atravessar mais material.
//     powder = 1 - exp(-density * 2 * stepSize)
//
//  Referências:
//    - A. Schneider, "The Real-time Volumetric Cloudscapes of Horizon: Zero
//      Dawn", SIGGRAPH 2015.
//    - S. Hillaire, "A Scalable and Production Ready Sky and Atmosphere
//      Rendering Technique", EGSR 2020.
// ═════════════════════════════════════════════════════════════════════════════

in vec2 vUV;

// Output: dois color attachments
layout(location = 0) out vec4 oCloudColor;       // RGB = cor acumulada, A = não usado
layout(location = 1) out float oTransmittance;   // transmittância final [0=opaco, 1=vazio]

// ── Uniforms ──────────────────────────────────────────────────────────────────

// Reconstrução de raios
uniform mat4  uInvView;
uniform mat4  uInvProj;
uniform vec3  uCameraPos;
uniform float uTime;
uniform vec2  uResolution;

// Noise textures
uniform sampler3D uShapeNoise;   // 128³ RGBA — Perlin-Worley + Worley
uniform sampler3D uDetailNoise;  // 32³  RGBA — Worley detail

// Parâmetros de nuvem
uniform float uCloudBottom;        // altitude base da camada (m)
uniform float uCloudTop;           // altitude topo da camada (m)
uniform float uCoverage;           // cobertura global [0,1]
uniform float uDensity;            // multiplicador de densidade
uniform float uShapeScale;         // frequência do shape noise
uniform float uDetailScale;        // frequência do detail noise
uniform float uDetailStrength;     // quanto o detail erode a forma base

// Vento
uniform vec3  uWindDir;
uniform float uWindSpeed;

// Iluminação
uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform float uSunIntensity;
uniform float uLightAbsorption;    // coeficiente de absorção para raio de luz
uniform float uCloudAbsorption;    // coeficiente de absorção para raio de câmara
uniform float uDarkEdgeFactor;     // intensidade do efeito powder/dark-edge

// Henyey-Greenstein
uniform float uHGForward;          // g ∈ [0, 1)  — lóbulo forward
uniform float uHGBackward;         // g ∈ (-1, 0] — lóbulo backward
uniform float uHGBlend;            // [0,1] mistura entre lóbulos

// Ray march
uniform int   uPrimarySteps;
uniform int   uLightSteps;
uniform float uMaxRayDist;

// Céu
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;

// ── Funções auxiliares ────────────────────────────────────────────────────────

// Remap: remapeia v do intervalo [lo,hi] para [newLo, newHi]
// Usa mix() para que funcione mesmo quando newLo > newHi (ex: fade invertido)
float remap(float v, float lo, float hi, float newLo, float newHi)
{
    float t = clamp((v - lo) / max(hi - lo, 1e-6), 0.0, 1.0);
    return mix(newLo, newHi, t);
}

// Henyey-Greenstein phase function
// θ = ângulo entre raio de câmara e direcção da luz
// g ∈ (-1, 1): g>0 = forward scattering, g<0 = backward scattering
float henyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// Phase function combinada (dois lóbulos)
float phaseFunction(float cosTheta)
{
    float pfForward  = henyeyGreenstein(cosTheta,  uHGForward);
    float pfBackward = henyeyGreenstein(cosTheta,  uHGBackward);
    return mix(pfForward, pfBackward, uHGBlend);
}

// ── Interseção raio-plano horizontal ─────────────────────────────────────────

// Retorna a distância ao plano y=altitude; -1 se não intersecta na direcção positiva
float rayPlane(vec3 ro, vec3 rd, float altitude)
{
    if (abs(rd.y) < 1e-6) return -1.0;
    float t = (altitude - ro.y) / rd.y;
    return t;
}

// ── Densidade das nuvens ──────────────────────────────────────────────────────
//
//  Amostrar as texturas 3D de ruído e construir o valor de densidade
//  para o ponto p (coordenada 3D no mundo).

float sampleCloudDensity(vec3 p)
{
    vec3 windOffset = uWindDir * uWindSpeed * uTime;

    float normAlt = remap(p.y, uCloudBottom, uCloudTop, 0.0, 1.0);
    float heightGrad = normAlt * remap(normAlt, 0.07, 0.14, 0.0, 1.0)
                     * remap(normAlt, 0.85, 1.0, 1.0, 0.0);
    heightGrad = clamp(heightGrad, 0.0, 1.0);
    if (heightGrad <= 0.0) return 0.0;

    vec3 samplePos = p * uShapeScale + windOffset * uShapeScale;
    vec4 shape = texture(uShapeNoise, samplePos);
    float shapeFBM = shape.r * 0.625 + shape.g * 0.250 + shape.b * 0.125;

    float cloudShape = remap(shapeFBM, 1.0 - uCoverage, 1.0, 0.0, 1.0);
    cloudShape *= heightGrad;
    if (cloudShape <= 0.0) return 0.0;

    vec3 detailSamplePos = p * uDetailScale + windOffset * uDetailScale * 0.5;
    vec4 detail = texture(uDetailNoise, detailSamplePos);
    float detailFBM = detail.r * 0.625 + detail.g * 0.250 + detail.b * 0.125;

    float detailMod = mix(detailFBM, 1.0 - detailFBM, clamp(normAlt * 10.0, 0.0, 1.0));
    float finalDensity = remap(cloudShape, detailMod * uDetailStrength, 1.0, 0.0, 1.0);

    return max(0.0, finalDensity * uDensity);
}

// ── Light march (sombra das nuvens sobre si próprias) ─────────────────────────
//
//  A partir de um ponto p, lança uLightSteps amostras na direcção do sol.
//  Acumula densidade → aplica Beer-Lambert → retorna energia luminosa restante.

float lightMarch(vec3 p)
{
    // Distância da amostra atual até sair da camada pelo topo (na direcção do sol)
    float tTop  = rayPlane(p, uSunDir, uCloudTop);
    if (tTop <= 0.0) return 1.0; // sol abaixo do horizonte

    float stepSize = tTop / float(uLightSteps);
    float totalDensity = 0.0;

    vec3 pos = p;
    for (int i = 0; i < uLightSteps; i++)
    {
        pos += uSunDir * stepSize;
        if (pos.y > uCloudTop || pos.y < uCloudBottom) break;
        totalDensity += sampleCloudDensity(pos) * stepSize;
    }

    // Beer-Lambert: transmittância ao longo do raio de luz
    float beersLaw = exp(-totalDensity * uLightAbsorption);

    // Powder / dark edge: efeito que escurece o interior das nuvens
    // Simula multiple scattering sem custo adicional
    float powder = 1.0 - exp(-totalDensity * uDarkEdgeFactor * 2.0);
    powder = mix(1.0, powder, clamp(dot(-uSunDir, vec3(0,1,0)), 0.0, 1.0));

    return max(beersLaw, beersLaw * powder * uDarkEdgeFactor);
}

// ── Ray march principal ───────────────────────────────────────────────────────

void main()
{
    // ── 1. Reconstruir raio a partir do pixel UV ──────────────────────────
    vec2 ndc  = vUV * 2.0 - 1.0;                   // [0,1] → [-1,1]
    vec4 clip = vec4(ndc, -1.0, 1.0);               // clip space
    vec4 eye  = uInvProj * clip;
    eye = vec4(eye.xy, -1.0, 0.0);
    vec3 rd   = normalize((uInvView * eye).xyz);    // direcção do raio (world space)
    vec3 ro   = uCameraPos;                         // origem do raio

    // ── 2. Interseção com a camada de nuvens (duas esferas horizontais) ──
    float tBottom = rayPlane(ro, rd, uCloudBottom);
    float tTop    = rayPlane(ro, rd, uCloudTop);

    // Determinar intervalo [tStart, tEnd] onde o raio está dentro da camada
    float tStart, tEnd;

    if (ro.y < uCloudBottom) {
        // Câmara abaixo da camada → entrar pelo fundo
        if (tBottom <= 0.0) { oCloudColor = vec4(0.0); oTransmittance = 1.0; return; }
        tStart = tBottom;
        tEnd   = (tTop > 0.0) ? tTop : tStart + (uCloudTop - uCloudBottom) / max(abs(rd.y), 0.001);
    } else if (ro.y > uCloudTop) {
        // Câmara acima da camada → entrar pelo topo
        if (tTop <= 0.0) { oCloudColor = vec4(0.0); oTransmittance = 1.0; return; }
        tStart = tTop;
        tEnd   = (tBottom > 0.0) ? tBottom : tStart + (uCloudTop - uCloudBottom) / max(abs(rd.y), 0.001);
    } else {
        // Câmara dentro da camada
        tStart = 0.0;
        float t1 = (tTop    > 0.0) ? tTop    : -1.0;
        float t2 = (tBottom > 0.0) ? tBottom : -1.0;
        if      (t1 > 0.0 && t2 > 0.0) tEnd = min(t1, t2);
        else if (t1 > 0.0)              tEnd = t1;
        else if (t2 > 0.0)              tEnd = t2;
        else { oCloudColor = vec4(0.0); oTransmittance = 1.0; return; }
    }

    tStart = max(tStart, 0.0);
    tEnd   = min(tEnd,   uMaxRayDist);
    if (tStart >= tEnd) { oCloudColor = vec4(0.0); oTransmittance = 1.0; return; }

    // ── 3. Ray march ──────────────────────────────────────────────────────
    float stepSize = (tEnd - tStart) / float(uPrimarySteps);

    // Offset aleatório para reduzir banding (dithering)
    // Usa posição do pixel como semente de hash simples
    float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    float t = tStart + jitter * stepSize;

    // Acumuladores
    vec3  cloudColor      = vec3(0.0);
    float transmittance   = 1.0;   // começa totalmente transparente

    // Coseno do ângulo entre raio e direcção do sol (para phase function)
    float cosTheta = dot(rd, uSunDir);
    float phase    = phaseFunction(cosTheta);

    for (int i = 0; i < uPrimarySteps; i++)
    {
        if (t > tEnd) break;
        if (transmittance < 0.01) break; // early exit — nuvem já é opaca

        vec3 pos = ro + rd * t;

        // Só amostrar dentro da camada
        if (pos.y >= uCloudBottom && pos.y <= uCloudTop)
        {
            float density = sampleCloudDensity(pos);

            if (density > 0.0)
            {
                // Light march: quanta luz do sol chega a este ponto
                float lightEnergy = lightMarch(pos);

                // Energia luminosa neste ponto = luz × phase × intensidade
                vec3 scatteredLight = uSunColor * uSunIntensity * lightEnergy * phase;

                // Transmittância deste segmento (Beer-Lambert)
                float segTransmit = exp(-density * uCloudAbsorption * stepSize);

                // Integral de scattering: acumula cor ponderada pela transmittância
                // usando integração de Euler:
                //   color += transmittance × scatteredLight × (1 - segTransmit) / absorption
                float absorption = max(density * uCloudAbsorption, 1e-5);
                cloudColor += transmittance * scatteredLight * (1.0 - segTransmit) / absorption;

                transmittance *= segTransmit;
            }
        }

        // Adaptar tamanho do passo: passos maiores onde há menos densidade
        // (otimização: evita sobresampling em regiões vazias)
        t += stepSize;
    }

    oCloudColor    = vec4(cloudColor, 1.0);
    oTransmittance = transmittance;
}
