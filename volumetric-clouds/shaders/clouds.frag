#version 420 core

in vec2 vUV;

layout(location = 0) out vec4 oCloudColor;
layout(location = 1) out float oTransmittance;

// ── Uniforms ──────────────────────────────────────────────────────────────────

uniform mat4  uInvView;
uniform mat4  uInvProj;
uniform vec3  uCameraPos;
uniform float uTime;
uniform vec2  uResolution;

uniform sampler3D uShapeNoise;
uniform sampler3D uDetailNoise;

// Textura do frame anterior para acumulação temporal
uniform sampler2D uPrevCloudColor;
uniform sampler2D uPrevCloudAlpha;
uniform float     uTemporalBlend;   // 0=só história, 1=só frame atual
uniform int       uFrameIndex;

uniform float uCloudBottom;
uniform float uCloudTop;
uniform float uCoverage;
uniform float uDensity;
uniform float uShapeScale;
uniform float uDetailScale;
uniform float uDetailStrength;

uniform vec3  uWindDir;
uniform float uWindSpeed;

uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform float uSunIntensity;
uniform float uLightAbsorption;
uniform float uCloudAbsorption;
uniform float uDarkEdgeFactor;

uniform float uHGForward;
uniform float uHGBackward;
uniform float uHGBlend;

uniform int   uPrimarySteps;
uniform int   uLightSteps;
uniform float uMaxRayDist;

uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;

// ── Funções base ──────────────────────────────────────────────────────────────

float remap(float v, float lo, float hi, float newLo, float newHi)
{
    float t = clamp((v - lo) / max(hi - lo, 1e-6), 0.0, 1.0);
    return mix(newLo, newHi, t);
}

float henyeyGreenstein(float cosTheta, float g)
{
    float g2 = g*g;
    return (1.0 - g2) / (4.0*3.14159265 * pow(max(1.0 + g2 - 2.0*g*cosTheta, 0.001), 1.5));
}

// Função de fase dupla (silver lining + back-scattering)
float phaseFunction(float cosTheta)
{
    float pf  = henyeyGreenstein(cosTheta,  uHGForward);
    float pb  = henyeyGreenstein(cosTheta,  uHGBackward);
    return mix(pf, pb, uHGBlend);
}

// Interleaved Gradient Noise — muito menos artefactos que ruído branco
float ign(vec2 pos)
{
    return fract(52.9829189 * fract(dot(pos, vec2(0.06711056, 0.00583715))));
}

float rayPlane(vec3 ro, vec3 rd, float altitude)
{
    if (abs(rd.y) < 1e-5) return -1.0;
    float t = (altitude - ro.y) / rd.y;
    return t;
}

// ── Densidade das nuvens ──────────────────────────────────────────────────────

float sampleCloudDensity(vec3 p)
{
    vec3 windOffset = uWindDir * uWindSpeed * uTime;

    float normAlt = remap(p.y, uCloudBottom, uCloudTop, 0.0, 1.0);

    // Perfil de altitude: sobe suavemente na base, decresce no topo
    float baseGrad = remap(normAlt, 0.0,  0.15, 0.0, 1.0);
    float topGrad  = remap(normAlt, 0.85, 1.0,  1.0, 0.0);
    float heightGrad = baseGrad * topGrad;
    heightGrad = clamp(heightGrad, 0.0, 1.0);
    if (heightGrad <= 0.0) return 0.0;

    // Shape noise (Perlin-Worley) — 4 canais para FBM
    vec3 sp = p * uShapeScale + windOffset * uShapeScale;
    vec4 shape = texture(uShapeNoise, sp);
    float shapeFBM = shape.r*0.625 + shape.g*0.250 + shape.b*0.100 + shape.a*0.025;

    // Cobertura: remap para que valores > (1-coverage) produzam nuvens
    float cloudShape = remap(shapeFBM, 1.0 - uCoverage, 1.0, 0.0, 1.0);
    cloudShape *= heightGrad;
    if (cloudShape <= 0.0) return 0.0;

    // Detail noise: erosão nas bordas
    vec3 dp = p * uDetailScale + windOffset * uDetailScale * 0.4;
    vec4 detail = texture(uDetailNoise, dp);
    float detailFBM = detail.r*0.625 + detail.g*0.250 + detail.b*0.125;

    // Nas bordas (cloudShape baixo) o detail erode mais
    float erosion = mix(detailFBM, 1.0 - detailFBM, clamp(normAlt * 8.0, 0.0, 1.0));
    float finalDensity = remap(cloudShape, erosion * uDetailStrength, 1.0, 0.0, 1.0);

    return max(0.0, finalDensity * uDensity);
}

// ── Light march (amostragem uniforme + powder + ambient) ─────────────────────

float lightMarch(vec3 p, float normAlt)
{
    float tTop = rayPlane(p, uSunDir, uCloudTop);
    if (tTop <= 0.0) return 1.0;

    // Amostragem uniforme dentro da camada [p → topo]
    float stepLen = tTop / float(uLightSteps);
    float totalDensity = 0.0;

    for (int i = 0; i < uLightSteps; i++) {
        vec3 pos = p + uSunDir * ((float(i) + 0.5) * stepLen);
        if (pos.y > uCloudTop || pos.y < uCloudBottom) break;
        totalDensity += sampleCloudDensity(pos) * stepLen;
    }

    // Beer-Lambert
    float beer = exp(-totalDensity * uLightAbsorption);

    // Powder/Dark-Edge: múltiplo espalhamento (borda escura na base)
    float powder = 1.0 - exp(-totalDensity * uDarkEdgeFactor * 2.0);
    float sunUp = clamp(dot(uSunDir, vec3(0,1,0)), 0.0, 1.0);
    float powderBlend = mix(0.75, 0.15, sunUp);
    return mix(beer, beer * powder, powderBlend);
}

// ── Ray march principal ───────────────────────────────────────────────────────

void main()
{
    // 1. Reconstruir raio
    vec2 ndc = vUV * 2.0 - 1.0;
    vec4 eye = uInvProj * vec4(ndc, -1.0, 1.0);
    eye = vec4(eye.xy, -1.0, 0.0);
    vec3 rd = normalize((uInvView * eye).xyz);
    vec3 ro = uCameraPos;

    // 2. Intervalo de intersecção com a camada
    float tBottom = rayPlane(ro, rd, uCloudBottom);
    float tTop    = rayPlane(ro, rd, uCloudTop);
    float tStart, tEnd;

    if (ro.y < uCloudBottom) {
        if (tBottom <= 0.0) { oCloudColor = vec4(0.0); oTransmittance = 1.0; return; }
        tStart = tBottom;
        tEnd   = (tTop > 0.0) ? tTop : tStart + (uCloudTop - uCloudBottom) / max(abs(rd.y), 0.001);
    } else if (ro.y > uCloudTop) {
        if (tTop <= 0.0) { oCloudColor = vec4(0.0); oTransmittance = 1.0; return; }
        tStart = tTop;
        tEnd   = (tBottom > 0.0) ? tBottom : tStart + (uCloudTop - uCloudBottom) / max(abs(rd.y), 0.001);
    } else {
        // Câmara dentro da camada — raios horizontais (rd.y≈0) nunca cruzam
        // o topo nem a base, mas devem marchar na horizontal até maxDist
        tStart = 0.0;
        float t1 = (tTop    > 0.0) ? tTop    : -1.0;
        float t2 = (tBottom > 0.0) ? tBottom : -1.0;
        if      (t1 > 0.0 && t2 > 0.0) tEnd = min(t1, t2);
        else if (t1 > 0.0)              tEnd = t1;
        else if (t2 > 0.0)              tEnd = t2;
        else                            tEnd = uMaxRayDist; // raio horizontal
    }

    tStart = max(tStart, 0.0);
    tEnd   = min(tEnd, uMaxRayDist);
    if (tStart >= tEnd) { oCloudColor = vec4(0.0); oTransmittance = 1.0; return; }

    // 3. Setup do ray march
    // Limitar o step a 150 m evita sub-amostragem em ângulos rasantes
    // (raios com grande tEnd-tStart teriam steps de 400-1000 m sem o cap)
    float effectiveLen = min(tEnd - tStart, float(uPrimarySteps) * 150.0);
    float stepSize = effectiveLen / float(uPrimarySteps);
    // IGN: padrão de baixa discrepância, muito menos artefactos que ruído branco
    float jitter = ign(gl_FragCoord.xy + vec2(float(uFrameIndex) * 1.618034));
    float t = tStart + jitter * stepSize;

    vec3  cloudColor    = vec3(0.0);
    float transmittance = 1.0;
    float cosTheta      = dot(rd, uSunDir);
    float phase         = phaseFunction(cosTheta);

    // 4. Ray march
    for (int i = 0; i < uPrimarySteps; i++)
    {
        if (t > tEnd) break;
        if (transmittance < 0.005) break;

        vec3 pos = ro + rd * t;

        if (pos.y >= uCloudBottom && pos.y <= uCloudTop)
        {
            float density = sampleCloudDensity(pos);

            if (density > 0.001)
            {
                float normAlt = remap(pos.y, uCloudBottom, uCloudTop, 0.0, 1.0);

                // Iluminação directa do sol
                float lightEnergy = lightMarch(pos, normAlt);
                vec3 sunLight = uSunColor * uSunIntensity * lightEnergy * phase;

                // Luz ambiente (ilumina base com cor do céu)
                float ambOcc  = remap(normAlt, 0.0, 0.5, 0.08, 1.0);
                vec3 ambLight = mix(uSkyHorizon * 0.6, uSkyZenith, normAlt) * 0.20 * ambOcc;

                vec3 totalLight = sunLight + ambLight;

                // Beer-Lambert deste segmento + integral analítica de scattering
                // (1 - segTransmit) dá o in-scattering correto sem divisão por σ_t
                float segTransmit = exp(-density * uCloudAbsorption * stepSize);
                cloudColor += transmittance * totalLight * (1.0 - segTransmit);
                transmittance *= segTransmit;
            }
        }

        // Passo adaptativo: maior onde há menos potencial de densidade
        // (fica mais curto dentro da camada, mais largo no vazio)
        t += stepSize;
    }

    // 5. Acumulação temporal — blend com frame anterior
    vec4  prevColor   = texture(uPrevCloudColor, vUV);
    float prevTransmit = texture(uPrevCloudAlpha, vUV).r;
    vec4  currColor   = vec4(cloudColor, 1.0);

    oCloudColor    = mix(prevColor,   currColor,           uTemporalBlend);
    oTransmittance = mix(prevTransmit, transmittance,      uTemporalBlend);
}
