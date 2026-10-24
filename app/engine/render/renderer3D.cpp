#include <chrono>
#include "renderer3D.hpp"
#include "../../utils/graphics/d3dUtil.hpp"
#include "../world/style/habitatStyle.hpp"
#include "../world/style/itemStyle.hpp"
#include "../world/style/pokemonStyle.hpp"
#include "undergroundGeometry.hpp"
#include "../../utils/core/crashUtil.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace {
    // Normal de un anillo del cuerpo del jugador: horizontal hacia fuera con algo de inclinación vertical.
    XMFLOAT3 ringNormal(float theta, float tilt) {
        const float x = std::cos(theta), z = std::sin(theta);
        const float n = std::sqrt(1.0f + tilt * tilt);
        return { x / n, tilt / n, z / n };
    }

    // Añade un triángulo orientado hacia fuera (cara frontal = sentido horario visto desde fuera) en una
    // forma convexa centrada en 'center'. Así no hay que acertar el orden de los vértices a mano.
    template <typename V>
    void addOutward(std::vector<uint16_t>& indices, const std::vector<V>& vertices, int a, int b, int c, const XMFLOAT3& center) {
        const XMVECTOR pa = XMLoadFloat3(&vertices[a].pos);
        const XMVECTOR pb = XMLoadFloat3(&vertices[b].pos);
        const XMVECTOR pc = XMLoadFloat3(&vertices[c].pos);
        const XMVECTOR normal = XMVector3Cross(XMVectorSubtract(pb, pa), XMVectorSubtract(pc, pa));
        const XMVECTOR centroid = XMVectorScale(XMVectorAdd(XMVectorAdd(pa, pb), pc), 1.0f / 3.0f);
        const XMVECTOR outward = XMVectorSubtract(centroid, XMLoadFloat3(&center));
        if (XMVectorGetX(XMVector3Dot(normal, outward)) < 0.0f) std::swap(b, c);
        indices.push_back(static_cast<uint16_t>(a));
        indices.push_back(static_cast<uint16_t>(b));
        indices.push_back(static_cast<uint16_t>(c));
    }

    XMMATRIX wallWorld(const Physics::World::Box& wall) {
        return XMMatrixScaling(wall.half.x * 2.0f, wall.half.y * 2.0f, wall.half.z * 2.0f) *
               XMMatrixTranslation(wall.center.x, wall.center.y, wall.center.z);
    }

    const XMFLOAT4 kWhite = { 1.0f, 1.0f, 1.0f, 0.0f };
    const XMFLOAT4 kWallTint = { 0.62f, 0.58f, 0.52f, 0.0f };
    const XMFLOAT4 kNoseTint = { 1.0f, 0.90f, 0.15f, 0.0f };

    // Mapa de sombras: una proyección ortográfica centrada en el jugador (el mundo es enorme) cuyo centro salta de texel en
    // texel, así las sombras no "nadan" al moverse. Vista desde la dirección de la luz activa.
    constexpr float kNodeDrawRange = 90.0f; // los materiales de recolección más lejos no se dibujan
    constexpr float kCaveDrawRange = 90.0f; // las galerías (y su decorado) más lejos no se dibujan
    constexpr float kLampRange = 50.0f;     // las luces del subsuelo más lejos no cuentan
    constexpr UINT kShadowMapSize = 3072;
    constexpr float kShadowHalfExtent = 80.0f; // radio de mundo que recibe sombras alrededor del jugador
    constexpr float kLightDistance = 400.0f;
    constexpr float kLightDepth = 500.0f;      // profundidad total del volumen de sombras (cubre desde el mar hasta la isla del cielo)
    constexpr float kFarPlane = 700.0f;
    constexpr float kTerrainDrawRange = 640.0f; // los trozos de terreno más lejos no se dibujan
    constexpr int kChunkCells = 32;             // celdas por lado de cada trozo de terreno (33 x 33 vértices: cabe en índices de 16 bits)
    constexpr float kFogDensity = 0.0020f;
    constexpr float kWaterFogDensity = 0.07f;
    constexpr float kOceanHalf = 1500.0f;
    constexpr float kHip = 0.62f;               // altura de la cadera del personaje
    constexpr float ROD_LENGTH = 1.8f;          // largo de la caña
    constexpr float kShadowNormalOffset = 0.05f; // evita el moteado en superficies casi paralelas a la luz
    constexpr float kShadowDepthBias = 0.0001f;

    constexpr const char* kShaderCode = R"(
        cbuffer Object : register(b0) {
            matrix WorldViewProj;
            matrix World;
            float4 Tint;
        }
        cbuffer Frame : register(b1) {
            matrix InvSky;
            matrix LightViewProj;
            float4 LightDir;
            float4 LightColor;
            float4 AmbientSky;
            float4 AmbientGround;
            float4 SunDir;
            float4 MoonDir;
            float4 SkyZenith;
            float4 SkyHorizon;
            float4 SkyParams;
            float4 ShadowParams; // x = texel, y = desplazamiento por la normal, z = sesgo
            float4 CameraPos;    // xyz = cámara, w = marea
            float4 Env;          // x = oscuridad, y = densidad de niebla, z = bajo el agua, w = radio del archipiélago
            float4 FogColor;
            float4 LampPos[8];   // xyz = posición, w = alcance
            float4 LampColor[8];
        }
        Texture2D ShadowMap : register(t0);
        SamplerComparisonState ShadowSampler : register(s0);

        // --- Objetos iluminados ---
        struct VS_IN { float3 pos : POSITION; float3 nrm : NORMAL; float4 col : COLOR; };
        struct PS_IN { float4 pos : SV_POSITION; float3 nrm : TEXCOORD0; float3 wpos : TEXCOORD1; float4 col : COLOR; };

        PS_IN VS(VS_IN input) {
            PS_IN output;
            output.pos = mul(float4(input.pos, 1.0f), WorldViewProj);
            output.wpos = mul(float4(input.pos, 1.0f), World).xyz;
            output.nrm = mul(input.nrm, (float3x3)World);
            output.col = input.col;
            return output;
        }

        // Fracción de luz directa que llega a un punto (1 = iluminado, 0 = en sombra). Promedia 3x3 lecturas
        // con comparación por hardware: el borde de la sombra (y la penumbra de una sombra tapada a medias) es suave.
        float Shadow(float3 wpos, float3 n) {
            float4 lp = mul(float4(wpos + n * ShadowParams.y, 1.0f), LightViewProj);
            float2 uv = lp.xy * float2(0.5f, -0.5f) + 0.5f;
            float depth = lp.z - ShadowParams.z;
            float sum = 0.0f;
            [unroll] for (int y = -1; y <= 1; ++y) {
                [unroll] for (int x = -1; x <= 1; ++x) {
                    sum += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + float2(x, y) * ShadowParams.x, depth);
                }
            }
            return sum / 9.0f;
        }

        // Niebla (o agua turbia bajo la superficie) y oscuridad de cuevas: solo se ve lo que queda cerca de la luz del jugador.
        float3 ApplyEnv(float3 color, float3 wpos) {
            float dist = length(wpos - CameraPos.xyz);
            float torch = saturate(1.0f - dist / 28.0f);
            float3 gain = torch * torch + 0.03f;
            [unroll] for (int i = 0; i < 8; ++i) { // lámparas y cristales: iluminan lo que tienen cerca
                float l = saturate(1.0f - length(wpos - LampPos[i].xyz) / max(LampPos[i].w, 0.01f));
                gain += LampColor[i].rgb * l * l;
            }
            color *= lerp(float3(1.0f, 1.0f, 1.0f), gain, Env.x);
            float fog = 1.0f - exp(-dist * Env.y);
            return lerp(color, FogColor.rgb, saturate(fog));
        }

        float4 PS(PS_IN input) : SV_Target {
            float3 n = normalize(input.nrm);
            float diffuse = saturate(dot(n, LightDir.xyz));
            float lit = 0.0f;
            if (diffuse > 0.0f) lit = Shadow(input.wpos, n);
            float3 ambient = lerp(AmbientGround.rgb, AmbientSky.rgb, n.y * 0.5f + 0.5f);
            // El alfa del vértice indica cuánto se tiñe (1 = todo, 0 = conserva su color); el alfa del tinte es el brillo propio.
            float3 albedo = lerp(input.col.rgb, input.col.rgb * Tint.rgb, input.col.a);
            float3 shaded = albedo * (ambient + LightColor.rgb * diffuse * lit);
            return float4(ApplyEnv(saturate(lerp(shaded, albedo, Tint.a)), input.wpos), 1.0f);
        }

        // --- Modelos con textura (coordenadas de textura en col.xy; col.z = 1 si las zonas rojas toman el color del tinte) ---
        Texture2D ModelTexture : register(t1);
        SamplerState ModelSampler : register(s1);

        float4 PS_MODEL(PS_IN input) : SV_Target {
            float3 n = normalize(input.nrm);
            float diffuse = saturate(dot(n, LightDir.xyz));
            float lit = 0.0f;
            if (diffuse > 0.0f) lit = Shadow(input.wpos, n);
            float3 ambient = lerp(AmbientGround.rgb, AmbientSky.rgb, n.y * 0.5f + 0.5f);
            float3 tex = ModelTexture.Sample(ModelSampler, input.col.xy).rgb;
            float redness = saturate((tex.r - max(tex.g, tex.b)) * 3.0f) * input.col.z;
            float3 albedo = lerp(tex, Tint.rgb * saturate(tex.r * 1.1f + 0.05f), redness);
            float3 shaded = albedo * (ambient + LightColor.rgb * diffuse * lit);
            return float4(ApplyEnv(saturate(lerp(shaded, albedo, Tint.a)), input.wpos), 1.0f);
        }

        // --- Brillo aditivo (rayo, silueta, destellos): más intenso en el borde; col.r atenúa a lo largo del rayo ---
        float4 PS_GLOW(PS_IN input) : SV_Target {
            float3 v = normalize(CameraPos.xyz - input.wpos);
            float rim = pow(1.0f - saturate(abs(dot(normalize(input.nrm), v))), 1.5f);
            float k = input.col.r * (0.55f + 0.9f * rim);
            return float4(Tint.rgb * Tint.a * k, 1.0f);
        }

        // --- Mapa de sombras: solo profundidad ---
        float4 VS_SHADOW(VS_IN input) : SV_POSITION {
            return mul(float4(input.pos, 1.0f), WorldViewProj);
        }

        // --- Cielo: degradado, sol, luna y estrellas, todo en un triángulo a pantalla completa ---
        struct SKY_OUT { float4 pos : SV_POSITION; float2 ndc : TEXCOORD0; };

        SKY_OUT VS_SKY(uint id : SV_VertexID) {
            float2 p = float2((id == 2) ? 3.0f : -1.0f, (id == 1) ? 3.0f : -1.0f);
            SKY_OUT output;
            output.pos = float4(p, 1.0f, 1.0f);
            output.ndc = p;
            return output;
        }

        float hash21(float2 p) {
            p = frac(p * float2(123.34f, 456.21f));
            p += dot(p, p + 45.32f);
            return frac(p.x * p.y);
        }

        float4 PS_SKY(SKY_OUT input) : SV_Target {
            float4 w = mul(float4(input.ndc, 1.0f, 1.0f), InvSky);
            float3 dir = normalize(w.xyz / w.w);

            float up = saturate(dir.y);
            float3 sky = lerp(SkyHorizon.rgb, SkyZenith.rgb, pow(up, 0.55f));

            float sunDot = dot(dir, SunDir.xyz);
            float warm = SkyParams.y;
            sky += warm * float3(0.95f, 0.42f, 0.12f) * pow(saturate(sunDot), 5.0f) * (1.0f - up);

            // Estrellas: celdas con un punto de brillo y parpadeo, giran con el cielo.
            float stars = SkyParams.x * smoothstep(0.0f, 0.15f, dir.y);
            if (stars > 0.01f) {
                float2 uv = float2(atan2(dir.y, dir.x) + SkyParams.z, asin(clamp(dir.z, -1.0f, 1.0f))) * 70.0f;
                float2 cell = floor(uv);
                float2 f = frac(uv) - 0.5f;
                float h = hash21(cell);
                if (h > 0.965f) {
                    float2 jitter = (float2(hash21(cell + 7.1f), hash21(cell + 3.7f)) - 0.5f) * 0.5f;
                    float size = lerp(0.10f, 0.22f, hash21(cell + 1.3f));
                    float star = 1.0f - smoothstep(0.0f, size, length(f - jitter));
                    float twinkle = 0.75f + 0.25f * sin(SkyParams.w * 3.0f + h * 100.0f);
                    sky += star * twinkle * stars * float3(0.95f, 0.97f, 1.0f);
                }
            }

            // Sol y luna (no se ven por debajo del horizonte).
            float above = smoothstep(-0.02f, 0.03f, dir.y);
            float sun = smoothstep(0.99880f, 0.99925f, sunDot);
            sky += above * sun * float3(1.6f, 1.45f, 1.1f);
            sky += above * pow(saturate(sunDot), 48.0f) * 0.30f * float3(1.0f, 0.85f, 0.6f) * saturate(SunDir.y * 4.0f + 0.4f);

            float moonDot = dot(dir, MoonDir.xyz);
            float moon = smoothstep(0.99900f, 0.99935f, moonDot);
            sky = lerp(sky, float3(0.92f, 0.94f, 1.0f), above * moon * saturate(MoonDir.y * 6.0f + 0.5f));
            sky += above * pow(saturate(moonDot), 220.0f) * 0.25f * float3(0.7f, 0.8f, 1.0f) * saturate(MoonDir.y * 4.0f);

            sky = lerp(sky, FogColor.rgb, Env.z);   // bajo el agua no hay cielo
            sky *= 1.0f - Env.x * 0.97f;            // en una cueva tampoco
            return float4(saturate(sky), 1.0f);
        }

        // --- Agua: olas, marea, corriente, transparencia según la profundidad, espuma y reflejo del cielo ---
        // Por vértice: pos = superficie base, nrm = (profundidad, corriente x, corriente z), col.a = tipo
        // (0 mar, 1 río, 2 lago, 3 charca de pantano, 4 cascada, 5 mar abierto hasta el horizonte, 6 agua de nube, 7 lava, 8 agua termal).
        struct WATER_IN { float4 pos : SV_POSITION; float3 wpos : TEXCOORD0; float3 info : TEXCOORD1; float kind : TEXCOORD2; };

        float WaveHeight(float2 p, float kind, float t) {
            float amp = kind > 7.5f ? 0.02f : kind < 0.5f ? 0.22f : kind < 1.5f ? 0.05f : kind < 2.5f ? 0.09f : kind < 3.5f ? 0.02f : 0.10f;
            float h = sin(dot(p, float2(0.55f, 0.35f)) + t * 1.3f) * 0.6f
                    + sin(dot(p, float2(-0.31f, 0.72f)) + t * 1.7f) * 0.3f
                    + sin(dot(p, float2(1.10f, -0.40f)) + t * 2.3f) * 0.1f;
            return h * amp;
        }

        WATER_IN VS_WATER(VS_IN input) {
            WATER_IN output;
            float kind = floor(input.col.a + 0.5f);
            bool sea = kind < 0.5f || (kind > 4.5f && kind < 5.5f);
            float3 p = input.pos;
            p.y += (sea ? CameraPos.w : 0.0f) + WaveHeight(p.xz, kind > 5.5f ? 3.0f : kind > 4.5f ? 0.0f : kind, SkyParams.w);
            output.pos = mul(float4(p, 1.0f), WorldViewProj);
            output.wpos = p;
            output.info = float3(input.nrm.x + (sea ? CameraPos.w : 0.0f), input.nrm.y, input.nrm.z);
            output.kind = kind;
            return output;
        }

        float4 PS_WATER(WATER_IN input) : SV_Target {
            float k = floor(input.kind + 0.5f);
            float2 pxz = input.wpos.xz;
            bool open = k > 4.5f && k < 5.5f, cloud = k > 5.5f && k < 6.5f, lava = k > 6.5f && k < 7.5f, hot = k > 7.5f;
            if (open) clip(length(pxz) - (Env.w - 8.0f)); // el mar abierto solo fuera del archipiélago
            float t = SkyParams.w;
            float kk = (cloud || lava) ? 3.0f : open ? 0.0f : k;

            float e = 0.35f;
            float h0 = WaveHeight(pxz, kk, t);
            float hx = WaveHeight(pxz + float2(e, 0.0f), kk, t);
            float hz = WaveHeight(pxz + float2(0.0f, e), kk, t);
            float3 n = normalize(float3(-(hx - h0) / e * 2.5f, 1.0f, -(hz - h0) / e * 2.5f));
            float3 v = normalize(CameraPos.xyz - input.wpos);
            float depth = input.info.x;
            float2 flow = input.info.yz;

            if (lava) { // roca fundida que se enfría río abajo: la costra oscura crece hasta que solo queda roca volcánica
                float cool = saturate((input.kind - 7.0f) / 0.45f);
                float2 q = pxz * 0.35f + flow * t * 0.25f;
                float crack = 0.5f + 0.5f * sin(q.x * 2.3f + sin(q.y * 1.7f + t * 0.6f) * 2.0f) * sin(q.y * 2.1f + sin(q.x * 1.3f - t * 0.5f) * 2.0f);
                float3 ember = lerp(float3(0.55f, 0.08f, 0.02f), float3(1.0f, 0.60f, 0.10f), crack);
                float3 crust = float3(0.10f, 0.06f, 0.05f);
                float3 lc = lerp(ember, crust, smoothstep(0.65f, 0.95f, 1.0f - crack));
                lc = lerp(lc, crust, saturate(cool * 1.3f) * saturate(cool * 1.3f));
                float glow = 1.0f - 0.85f * cool;
                float alpha = 1.0f - smoothstep(0.7f, 1.0f, cool);
                return float4(ApplyEnv(lc * 0.35f, input.wpos) + lc * 0.9f * glow, alpha);
            }
            float3 shallow, deep;
            float full;
            if (hot)                  { shallow = float3(0.58f, 0.90f, 0.86f); deep = float3(0.16f, 0.60f, 0.64f); full = 2.0f; }
            else if (cloud)           { shallow = float3(0.93f, 0.96f, 1.0f); deep = float3(0.60f, 0.73f, 0.95f); full = 2.0f; }
            else if (k < 0.5f || open) { shallow = float3(0.30f, 0.80f, 0.84f); deep = float3(0.03f, 0.18f, 0.42f); full = 7.0f; }
            else if (k < 1.5f)        { shallow = float3(0.38f, 0.74f, 0.74f); deep = float3(0.10f, 0.36f, 0.52f); full = 4.5f; }
            else if (k < 2.5f)        { shallow = float3(0.30f, 0.68f, 0.74f); deep = float3(0.06f, 0.28f, 0.46f); full = 8.0f; }
            else if (k < 3.5f)        { shallow = float3(0.30f, 0.46f, 0.16f); deep = float3(0.10f, 0.24f, 0.10f); full = 2.0f; }
            else                      { shallow = float3(0.80f, 0.92f, 0.96f); deep = float3(0.45f, 0.70f, 0.85f); full = 3.0f; }
            float tdep = saturate(depth / full);
            float3 base = lerp(shallow, deep, sqrt(tdep));
            float alpha = cloud ? lerp(0.55f, 0.88f, sqrt(tdep)) : (k > 2.5f && k < 3.5f) ? lerp(0.55f, 0.92f, tdep) : (k > 3.5f && k < 4.5f) ? 0.85f : lerp(0.28f, 0.96f, sqrt(tdep));
            alpha *= smoothstep(0.0f, 0.25f, depth); // en la orilla es transparente
            if (open) alpha = 1.0f;

            float diff = saturate(dot(n, LightDir.xyz));
            float3 color = base * (AmbientSky.rgb + LightColor.rgb * diff * 0.8f);
            float fres = 0.04f + 0.70f * pow(1.0f - saturate(dot(n, v)), 4.0f);
            color = lerp(color, SkyHorizon.rgb * 0.9f + AmbientSky.rgb * 0.2f, fres);
            float spec = pow(saturate(dot(reflect(-LightDir.xyz, n), v)), 90.0f);
            color += LightColor.rgb * spec * ((k > 2.5f && k < 3.5f) ? 0.2f : 1.2f);

            // Espuma en la orilla, rayas de corriente y cascadas.
            float foam = 0.0f;
            if (k < 2.5f) {
                float edge = 1.0f - smoothstep(0.0f, 0.5f, depth);
                float wob = 0.5f + 0.5f * sin(t * 1.8f + pxz.x * 0.9f + pxz.y * 0.7f);
                foam = edge * (0.4f + 0.6f * wob);
            }
            float speed = length(flow);
            if (speed > 0.05f) {
                float2 d = flow / speed;
                float s = sin(dot(pxz, d) * 2.6f - t * speed * 4.0f + sin(dot(pxz, float2(-d.y, d.x)) * 1.7f) * 1.3f);
                foam += saturate(s - 0.7f) * 0.5f * saturate(speed);
            }
            if (k > 3.5f && k < 4.5f) foam += 0.5f + 0.4f * sin(pxz.x * 2.1f + pxz.y * 3.0f - t * 9.0f);
            color = lerp(color, float3(0.95f, 0.98f, 1.0f), saturate(foam) * 0.85f);
            alpha = saturate(alpha + foam * 0.5f + fres * 0.25f);
            return float4(ApplyEnv(color, input.wpos), alpha);
        }
    )";
}

Renderer3D::Mesh Renderer3D::createMesh(ID3D11Device* device, const void* vertices, UINT vertexCount, UINT stride,
                                        const std::vector<uint16_t>& indices) {
    Mesh mesh;
    mesh.indexCount = static_cast<UINT>(indices.size());
    mesh.stride = stride;
    mesh.vertices = D3dUtil::createBuffer(device, stride * vertexCount, D3D11_BIND_VERTEX_BUFFER,
        vertices, D3D11_USAGE_IMMUTABLE, "CreateBuffer (vertex)");
    mesh.indices = D3dUtil::createBuffer(device, static_cast<UINT>(sizeof(uint16_t) * indices.size()), D3D11_BIND_INDEX_BUFFER,
        indices.data(), D3D11_USAGE_IMMUTABLE, "CreateBuffer (index)");
    return mesh;
}

// Trozos de malla de un campo de nodos (n x n, un nodo cada Heightfield::CELL metros): cada trozo son kChunkCells celdas por lado con
// vértices propios (cabe en 16 bits) y solo las celdas que 'keepCell' acepta. vertexAt(col, row) da el vértice de un nodo.
template <typename VertexFn, typename KeepFn>
std::vector<Renderer3D::Chunk> Renderer3D::buildChunks(ID3D11Device* device, int nodes, VertexFn vertexAt, KeepFn keepCell) {
    std::vector<Chunk> chunks;
    const int cells = nodes - 1;
    const int count = (cells + kChunkCells - 1) / kChunkCells;
    constexpr int side = kChunkCells + 1;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    for (int cz = 0; cz < count; ++cz) {
        for (int cx = 0; cx < count; ++cx) {
            const int col0 = cx * kChunkCells, row0 = cz * kChunkCells;
            indices.clear();
            for (int r = 0; r < kChunkCells && row0 + r < cells; ++r) {
                for (int c = 0; c < kChunkCells && col0 + c < cells; ++c) {
                    if (!keepCell(col0 + c, row0 + r)) continue;
                    const int i0 = r * side + c, i1 = i0 + 1, i2 = i0 + side, i3 = i2 + 1;
                    for (const int i : { i0, i2, i1, i1, i2, i3 }) indices.push_back(static_cast<uint16_t>(i));
                }
            }
            if (indices.empty()) continue;

            vertices.clear();
            XMFLOAT3 lo = { 1.0e9f, 1.0e9f, 1.0e9f }, hi = { -1.0e9f, -1.0e9f, -1.0e9f };
            for (int r = 0; r < side; ++r) {
                for (int c = 0; c < side; ++c) {
                    const Vertex v = vertexAt((std::min)(col0 + c, nodes - 1), (std::min)(row0 + r, nodes - 1));
                    vertices.push_back(v);
                    lo = { (std::min)(lo.x, v.pos.x), (std::min)(lo.y, v.pos.y), (std::min)(lo.z, v.pos.z) };
                    hi = { (std::max)(hi.x, v.pos.x), (std::max)(hi.y, v.pos.y), (std::max)(hi.z, v.pos.z) };
                }
            }
            Chunk chunk;
            chunk.mesh = createMesh(device, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(Vertex), indices);
            chunk.center = { (lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f };
            chunk.radius = 0.5f * std::sqrt((hi.x - lo.x) * (hi.x - lo.x) + (hi.y - lo.y) * (hi.y - lo.y) + (hi.z - lo.z) * (hi.z - lo.z));
            chunks.push_back(std::move(chunk));
        }
    }
    return chunks;
}

// Relieve del mundo. Color según el hábitat y la altura: arena junto al mar, roca en lo empinado, nieve y hielo en las cumbres
// (por encima de la línea de nieve) y fondo de arena o fango bajo el agua (el agua es una superficie aparte, translúcida).
// Los nodos fuera del archipiélago y de las regiones no se dibujan, ni lo que queda vacío alrededor de la isla del cielo.
std::vector<Renderer3D::Chunk> Renderer3D::createTerrain(ID3D11Device* device, const HabitatMap& habitats, const Physics::World& world, const GameData& data) {
    const Physics::Heightfield& terrain = world.terrain;
    constexpr XMFLOAT3 DEFAULT_GROUND = { 0.22f, 0.62f, 0.23f };
    constexpr XMFLOAT3 ROCK = { 0.46f, 0.44f, 0.42f };
    constexpr XMFLOAT3 SNOW = { 0.94f, 0.96f, 1.0f };
    constexpr XMFLOAT3 ICE = { 0.74f, 0.88f, 0.98f };
    constexpr XMFLOAT3 SAND = { 0.80f, 0.74f, 0.52f };
    constexpr XMFLOAT3 MUD = { 0.26f, 0.32f, 0.34f };
    std::vector<XMFLOAT3> habitatColors;
    for (const Habitat& habitat : data.habitats) habitatColors.push_back(HabitatStyle::color(habitat.name));

    const auto mix = [](const XMFLOAT3& a, const XMFLOAT3& b, float t) {
        t = (std::clamp)(t, 0.0f, 1.0f);
        return XMFLOAT3{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
    };
    const auto smooth = [](float a, float b, float x) {
        const float t = (std::clamp)((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    std::vector<float> weights;
    const int nodes = terrain.count();
    const auto vertexAt = [&](int col, int row) {
        const float x = terrain.nodeCoord(col), z = terrain.nodeCoord(row);
        const float height = terrain.at(col, row);
        float gx, gz;
        terrain.gradient(x, z, gx, gz);
        const float slope = std::sqrt(gx * gx + gz * gz);
        const float length = std::sqrt(gx * gx + 1.0f + gz * gz);

        XMFLOAT3 color;
        if (height < 0.0f) { // fondo: arena en lo somero, fango en lo hondo
            color = mix(SAND, MUD, -height / 10.0f);
            habitats.weights(x, z, weights);
            float seabed = 0.0f; // arrecife, algas y volcán submarino tiñen el fondo
            XMFLOAT3 tint = { 0.0f, 0.0f, 0.0f };
            for (size_t i = 0; i < weights.size() && i < habitatColors.size(); ++i) {
                if (!data.habitats[i].seabed() && data.habitats[i].placement != HabitatPlacement::VENT) continue;
                seabed += weights[i];
                tint = { tint.x + weights[i] * habitatColors[i].x, tint.y + weights[i] * habitatColors[i].y, tint.z + weights[i] * habitatColors[i].z };
            }
            if (seabed > 0.01f) color = mix(color, { tint.x / seabed, tint.y / seabed, tint.z / seabed }, seabed * 0.85f);
        } else {
            color = habitatColors.empty() ? DEFAULT_GROUND : XMFLOAT3{ 0.0f, 0.0f, 0.0f };
            habitats.weights(x, z, weights);
            for (size_t i = 0; i < weights.size() && i < habitatColors.size(); ++i) {
                color.x += weights[i] * habitatColors[i].x;
                color.y += weights[i] * habitatColors[i].y;
                color.z += weights[i] * habitatColors[i].z;
            }
            if (!habitats.regionAt(x, z)) {
                color = mix(color, SAND, smooth(0.5f, 0.0f, height) * 0.6f);           // la tierra pegada al mar es arenosa
                const float feature = habitats.featureWeight(x, z);                     // volcanes y glaciares conservan su color
                const float rock = smooth(0.55f, 1.0f, slope) * (1.0f - feature);      // lo empinado es roca
                color = mix(color, ROCK, rock);
                const float line = HabitatMap::SNOW_LINE + 2.5f * std::sin(x * 0.11f) * std::cos(z * 0.09f); // línea de nieve irregular
                const float snow = smooth(line - 2.0f, line + 2.5f, height) * (1.0f - 0.65f * rock) * (1.0f - feature);
                color = mix(color, SNOW, snow);
                color = mix(color, ICE, smooth(line + 10.0f, line + 22.0f, height) * (0.35f + 0.65f * rock)); // y hielo más arriba
            }
        }
        const float tone = 0.96f + 0.04f * std::sin(x * 1.7f) * std::cos(z * 1.3f); // variación fina para que el relieve se lea
        return Vertex{ { x, height, z }, { -gx / length, 1.0f / length, -gz / length }, { color.x * tone, color.y * tone, color.z * tone, 1.0f } };
    };
    const auto keepCell = [&](int col, int row) {
        const float x = terrain.nodeCoord(col) + Physics::Heightfield::CELL * 0.5f, z = terrain.nodeCoord(row) + Physics::Heightfield::CELL * 0.5f;
        if (!habitats.playable(x, z) || habitats.under().hole(col, row)) return false; // la boca de una galería la dibuja el subsuelo
        const float void_ = HabitatMap::SKY_VOID * 0.5f;
        if (const HabitatMap::Region* region = habitats.regionAt(x, z); region && region->kind == HabitatPlacement::SKY) {
            // Más allá del mar de nubes el relieve cae al vacío: no se dibuja (lo tapa la base de nube de createSkyBase).
            const float limit = region->floorY - HabitatMap::SKY_SEA_DEPTH - 1.5f;
            if (terrain.at(col, row) < limit && terrain.at(col + 1, row) < limit && terrain.at(col, row + 1) < limit && terrain.at(col + 1, row + 1) < limit) return false;
        }
        return terrain.at(col, row) > void_ && terrain.at(col + 1, row) > void_ && terrain.at(col, row + 1) > void_ && terrain.at(col + 1, row + 1) > void_;
    };
    return buildChunks(device, nodes, vertexAt, keepCell);
}

// Superficie del agua: una malla plana a la altura del agua en cada nodo con agua (y los de su orilla), con la profundidad, la corriente y el
// tipo de agua en cada vértice; el shader pone las olas, la marea, la transparencia y la espuma.
std::vector<Renderer3D::Chunk> Renderer3D::createWater(ID3D11Device* device, const HabitatMap& habitats, const Physics::World& world) {
    const Physics::Heightfield& ground = world.terrain;
    const Physics::Heightfield& level = world.water;
    const int n = ground.count();
    constexpr float WET = 0.03f;
    const auto wet = [&](int c, int r) { return c >= 0 && r >= 0 && c < n && r < n && level.at(c, r) - ground.at(c, r) > WET; };
    const auto code = [](uint8_t kind) {
        switch (static_cast<HabitatMap::Water>(kind)) {
            case HabitatMap::RIVER: case HabitatMap::UPPER_RIVER: return 1.0f;
            case HabitatMap::LAKE: case HabitatMap::UPPER_LAKE:   return 2.0f;
            case HabitatMap::POND:                                return 3.0f;
            case HabitatMap::CASCADE:                             return 4.0f;
            case HabitatMap::CLOUD:                               return 6.0f;
            case HabitatMap::LAVA:                                return 7.0f;
            case HabitatMap::HOT:                                 return 8.0f;
            default:                                              return 0.0f;
        }
    };

    const auto codeAt = [&](int col, int row) { // la lava lleva además cuánto se ha enfriado (parte decimal, hasta 0,45)
        float k = code(habitats.kindAtNode(col, row));
        if (k > 6.5f && k < 7.5f) k += 0.45f * habitats.lavaCoolAt(col, row);
        return k;
    };
    const auto vertexAt = [&](int col, int row) {
        float y = level.at(col, row), depth = (std::max)(0.0f, level.at(col, row) - ground.at(col, row)), kind = 0.0f;
        if (wet(col, row)) kind = codeAt(col, row);
        else { // el borde de la orilla toma el nivel y el tipo del agua que tiene al lado
            float sum = 0.0f;
            int count = 0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (!wet(col + dc, row + dr)) continue;
                    sum += level.at(col + dc, row + dr);
                    kind = codeAt(col + dc, row + dr);
                    ++count;
                }
            }
            if (count > 0) y = sum / static_cast<float>(count);
            depth = 0.0f;
        }
        return Vertex{ { ground.nodeCoord(col), y, ground.nodeCoord(row) }, { depth, world.flowX.at(col, row), world.flowZ.at(col, row) }, { 1.0f, 1.0f, 1.0f, kind } };
    };
    const auto keepCell = [&](int col, int row) { return wet(col, row) || wet(col + 1, row) || wet(col, row + 1) || wet(col + 1, row + 1); };
    return buildChunks(device, n, vertexAt, keepCell);
}

// Galerías de cuevas y minas (UndergroundGeometry): cada lote de geometría es un trozo que se descarta si queda lejos.
std::vector<Renderer3D::Chunk> Renderer3D::createUnderground(ID3D11Device* device, const HabitatMap& habitats, const GameData& data) {
    std::vector<Chunk> chunks;
    for (const UndergroundGeometry::Batch& batch : UndergroundGeometry::build(habitats, data)) {
        Chunk chunk;
        chunk.mesh = createMesh(device, batch.vertices.data(), static_cast<UINT>(batch.vertices.size()), sizeof(Vertex), batch.indices);
        chunk.center = batch.center;
        chunk.radius = batch.radius;
        chunks.push_back(std::move(chunk));
    }
    return chunks;
}

// Agua de las grutas: como la superficie del agua, pero sobre las capas subterráneas (tipo de agua 2, como un lago tranquilo).
std::vector<Renderer3D::Chunk> Renderer3D::createCaveWater(ID3D11Device* device, const HabitatMap& habitats) {
    const Underground& under = habitats.under();
    const Physics::Heightfield& floor = under.floorField();
    const Physics::Heightfield& level = under.waterField();
    const int n = floor.count();
    constexpr float WET = 0.03f;
    const auto wet = [&](int c, int r) { return c >= 0 && r >= 0 && c < n && r < n && under.nodeDefined(c, r) && level.at(c, r) - floor.at(c, r) > WET; };
    const auto vertexAt = [&](int col, int row) {
        float y = level.at(col, row), depth = (std::max)(0.0f, level.at(col, row) - floor.at(col, row)), kind = 2.0f;
        if (!wet(col, row)) { // el borde de la orilla toma el nivel del agua que tiene al lado
            float sum = 0.0f;
            int count = 0;
            for (int dr = -1; dr <= 1; ++dr) for (int dc = -1; dc <= 1; ++dc) if (wet(col + dc, row + dr)) { sum += level.at(col + dc, row + dr); ++count; }
            y = count > 0 ? sum / static_cast<float>(count) : 0.0f;
            depth = 0.0f;
        }
        return Vertex{ { floor.nodeCoord(col), y, floor.nodeCoord(row) }, { depth, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, kind } };
    };
    const auto keepCell = [&](int col, int row) { return wet(col, row) || wet(col + 1, row) || wet(col, row + 1) || wet(col + 1, row + 1); };
    return buildChunks(device, n, vertexAt, keepCell);
}

// Base de nube de cada isla del cielo: una sola malla ligera por isla (un anillo abultado que sobresale del borde del mar de nubes y se
// recoge hacia abajo hasta una punta) en vez de la roca o el abismo; la isla parece apoyada en una nube. Colores y lóbulos fijos.
std::vector<Renderer3D::Chunk> Renderer3D::createSkyBase(ID3D11Device* device, const HabitatMap& habitats) {
    constexpr int SEGMENTS = 72, RINGS = 8;              // anillos desde el borde (0) hasta la punta
    constexpr float LIP = 6.0f;                          // cuánto sobresale la nube del borde del mar
    constexpr XMFLOAT3 TOP = { 0.98f, 0.98f, 1.0f }, BOTTOM = { 0.70f, 0.76f, 0.88f };
    std::vector<Chunk> chunks;
    for (const HabitatMap::Region& region : habitats.regions()) {
        if (region.kind != HabitatPlacement::SKY) continue;
        const float level = region.floorY - 0.35f;      // nivel del mar de nubes
        const float depth = region.thickness; // grosor del lente de nube: pequeño frente a su anchura
        const XMFLOAT3 core = { region.x, level - depth * 0.5f, region.z };
        std::vector<Vertex> vertices;
        vertices.reserve(SEGMENTS * (RINGS + 1) + 1);
        for (int k = 0; k <= RINGS; ++k) {
            const float t = k <= 1 ? 0.0f : static_cast<float>(k - 1) / static_cast<float>(RINGS - 1); // 0 en el borde, 1 en la punta
            const XMFLOAT3 tone = { TOP.x + (BOTTOM.x - TOP.x) * t, TOP.y + (BOTTOM.y - TOP.y) * t, TOP.z + (BOTTOM.z - TOP.z) * t };
            for (int i = 0; i < SEGMENTS; ++i) {
                const float a = static_cast<float>(i) / SEGMENTS * XM_2PI;
                const float ca = std::cos(a), sa = std::sin(a);
                const float edge = region.seaRadius * HabitatMap::skyShape(region, ca, sa);
                const float lump = 1.0f + 0.10f * std::sin(3.0f * a + region.lobe[0] + 2.0f * k) + 0.09f * std::sin(7.0f * a + region.lobe[1] + 1.3f * k) + 0.08f * std::sin(13.0f * a + region.lobe[2] + 0.7f * k) + 0.05f * std::sin(23.0f * a + region.lobe[0] * 2.0f + 0.9f * k); // borde de nube abultado
                float radius, y;
                if (k == 0) { radius = edge - 0.5f; y = level - 0.4f; }                       // bajo la superficie del mar
                else if (k == 1) { radius = edge + LIP * lump; y = level - 1.6f; }            // el borde abultado
                else { const float s = t * 1.5707963f; radius = (edge + LIP) * std::cos(s) * lump; y = level - 1.6f - (depth - 1.6f) * std::sin(s); } // el vientre del lente: elipse
                const float shade = 0.94f + 0.06f * std::sin(5.0f * a + 1.7f * k);
                vertices.push_back({ { region.x + ca * radius, y, region.z + sa * radius }, { ca, 0.0f, sa }, { tone.x * shade, tone.y * shade, tone.z * shade, 1.0f } });
            }
        }
        const int tip = static_cast<int>(vertices.size());
        vertices.push_back({ { region.x, level - depth - 2.0f, region.z }, { 0.0f, -1.0f, 0.0f }, { BOTTOM.x * 0.95f, BOTTOM.y * 0.95f, BOTTOM.z * 0.95f, 1.0f } });

        // Normales por diferencias entre vecinos del anillo y de la columna, orientadas hacia fuera del centro de la nube.
        const auto at = [&](int k, int i) -> const Vertex& { return vertices[static_cast<size_t>((std::clamp)(k, 0, RINGS)) * SEGMENTS + ((i % SEGMENTS) + SEGMENTS) % SEGMENTS]; };
        for (int k = 0; k <= RINGS; ++k) {
            for (int i = 0; i < SEGMENTS; ++i) {
                const XMVECTOR along = XMVectorSubtract(XMLoadFloat3(&at(k, i + 1).pos), XMLoadFloat3(&at(k, i - 1).pos));
                const XMVECTOR down = XMVectorSubtract(XMLoadFloat3(&at((std::min)(k + 1, RINGS), i).pos), XMLoadFloat3(&at((std::max)(k - 1, 0), i).pos));
                XMVECTOR n = XMVector3Normalize(XMVector3Cross(down, along));
                Vertex& v = vertices[static_cast<size_t>(k) * SEGMENTS + i];
                const XMVECTOR outward = XMVectorSubtract(XMLoadFloat3(&v.pos), XMLoadFloat3(&core));
                if (XMVectorGetX(XMVector3Dot(n, outward)) < 0.0f) n = XMVectorNegate(n);
                XMStoreFloat3(&v.normal, n);
            }
        }

        std::vector<uint16_t> indices;
        const auto push = [&](int a, int b, int c) { // cara frontal hacia fuera del centro de la nube
            const XMVECTOR pa = XMLoadFloat3(&vertices[a].pos), pb = XMLoadFloat3(&vertices[b].pos), pc = XMLoadFloat3(&vertices[c].pos);
            const XMVECTOR cross = XMVector3Cross(XMVectorSubtract(pb, pa), XMVectorSubtract(pc, pa));
            const XMVECTOR centroid = XMVectorScale(XMVectorAdd(XMVectorAdd(pa, pb), pc), 1.0f / 3.0f);
            const bool facing = XMVectorGetX(XMVector3Dot(cross, XMVectorSubtract(centroid, XMLoadFloat3(&core)))) >= 0.0f;
            indices.push_back(static_cast<uint16_t>(a));
            indices.push_back(static_cast<uint16_t>(facing ? b : c));
            indices.push_back(static_cast<uint16_t>(facing ? c : b));
        };
        for (int k = 0; k < RINGS; ++k) {
            for (int i = 0; i < SEGMENTS; ++i) {
                const int j = (i + 1) % SEGMENTS;
                const int a = k * SEGMENTS + i, b = k * SEGMENTS + j, c = (k + 1) * SEGMENTS + i, d = (k + 1) * SEGMENTS + j;
                push(a, c, b);
                push(b, c, d);
            }
        }
        for (int i = 0; i < SEGMENTS; ++i) push(RINGS * SEGMENTS + i, tip, RINGS * SEGMENTS + (i + 1) % SEGMENTS);

        Chunk chunk;
        chunk.mesh = createMesh(device, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(Vertex), indices);
        chunk.center = core;
        chunk.radius = region.seaRadius * 1.5f + depth;
        chunks.push_back(std::move(chunk));
    }
    return chunks;
}

// El mar hasta el horizonte: un único cuadrado enorme (tipo de agua 5); el shader solo lo pinta fuera del archipiélago.
Renderer3D::Mesh Renderer3D::createOcean(ID3D11Device* device) {
    const float h = kOceanHalf;
    const Vertex vertices[4] = {
        { { -h, 0.0f, -h }, { 50.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 5.0f } },
        { { -h, 0.0f,  h }, { 50.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 5.0f } },
        { {  h, 0.0f, -h }, { 50.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 5.0f } },
        { {  h, 0.0f,  h }, { 50.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 5.0f } },
    };
    return createMesh(device, vertices, 4, sizeof(Vertex), { 0, 1, 2, 2, 1, 3 });
}

// Cono invertido unitario (radio 1 arriba, punta a -1 abajo): la base rocosa de la isla del cielo, más oscura cuanto más abajo.
Renderer3D::Mesh Renderer3D::createCone(ID3D11Device* device) {
    constexpr int segments = 20;
    std::vector<Vertex> vertices;
    for (int i = 0; i < segments; ++i) {
        const float theta = static_cast<float>(i) / segments * XM_2PI;
        const float wobble = 0.9f + 0.1f * std::sin(theta * 5.0f); // el borde no es un círculo perfecto
        vertices.push_back({ { std::cos(theta) * wobble, 0.0f, std::sin(theta) * wobble }, ringNormal(theta, -0.6f), { 0.52f, 0.44f, 0.36f, 1.0f } });
    }
    for (int i = 0; i < segments; ++i) {
        const float theta = (static_cast<float>(i) + 0.5f) / segments * XM_2PI;
        vertices.push_back({ { std::cos(theta) * 0.55f, -0.5f, std::sin(theta) * 0.55f }, ringNormal(theta, -0.6f), { 0.40f, 0.34f, 0.30f, 1.0f } });
    }
    vertices.push_back({ { 0.0f, -1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.28f, 0.24f, 0.22f, 1.0f } });
    const int tip = 2 * segments;
    const XMFLOAT3 inside = { 0.0f, -0.3f, 0.0f };
    std::vector<uint16_t> indices;
    for (int i = 0; i < segments; ++i) {
        const int j = (i + 1) % segments;
        addOutward(indices, vertices, i, j, segments + i, inside);
        addOutward(indices, vertices, j, segments + j, segments + i, inside);
        addOutward(indices, vertices, segments + i, segments + j, tip, inside);
    }
    return createMesh(device, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(Vertex), indices);
}

// Cubo unitario blanco centrado en el origen (el color lo pone el tinte y el volumen la iluminación).
Renderer3D::Mesh Renderer3D::createCube(ID3D11Device* device) {
    struct Face { XMFLOAT3 n, u, v; float shade; };
    const Face faces[6] = {
        { { 0, 1, 0 },  { 1, 0, 0 }, { 0, 0, 1 }, 1.00f },
        { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, 0.45f },
        { { 1, 0, 0 },  { 0, 1, 0 }, { 0, 0, 1 }, 0.80f },
        { { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, 0.65f },
        { { 0, 0, 1 },  { 1, 0, 0 }, { 0, 1, 0 }, 0.90f },
        { { 0, 0, -1 }, { 1, 0, 0 }, { 0, 1, 0 }, 0.70f },
    };

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(24);
    indices.reserve(36);

    const XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
    for (const Face& f : faces) {
        const XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        const int first = static_cast<int>(vertices.size());
        for (int k = 0; k < 4; ++k) {
            const float su = (k & 1) ? 0.5f : -0.5f;
            const float sv = (k & 2) ? 0.5f : -0.5f;
            vertices.push_back({ { f.n.x * 0.5f + f.u.x * su + f.v.x * sv,
                                   f.n.y * 0.5f + f.u.y * su + f.v.y * sv,
                                   f.n.z * 0.5f + f.u.z * su + f.v.z * sv }, f.n, color });
        }
        addOutward(indices, vertices, first, first + 1, first + 2, center);
        addOutward(indices, vertices, first + 2, first + 1, first + 3, center);
    }
    return createMesh(device, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(Vertex), indices);
}

// Pokéball unitaria: la mitad superior (alfa 1) se tiñe con el color del tipo, el ecuador es una banda oscura y la
// mitad inferior es blanca. Así se nota cómo se inclina al tambalearse.
Renderer3D::Mesh Renderer3D::createSphere(ID3D11Device* device, bool dome) {
    constexpr int rings = 16;
    constexpr int segments = 16;
    constexpr int bandFirst = 7, bandLast = 9;

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve((rings + 1) * segments);
    indices.reserve(rings * segments * 6);

    const int last = dome ? rings / 2 : rings; // la semiesfera llega hasta el ecuador
    for (int r = 0; r <= last; ++r) {
        const float phi = XM_PI * static_cast<float>(r) / rings; // 0 = polo superior
        const float y = std::cos(phi);
        const float ring = std::sin(phi);
        const XMFLOAT4 color = dome ? XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f } : r < bandFirst ? XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f }
                             : r <= bandLast ? XMFLOAT4{ 0.07f, 0.07f, 0.08f, 0.0f }
                                             : XMFLOAT4{ 0.95f, 0.95f, 0.95f, 0.0f };
        for (int s = 0; s < segments; ++s) {
            const float theta = static_cast<float>(s) / segments * XM_2PI;
            const XMFLOAT3 p = { std::cos(theta) * ring, y, std::sin(theta) * ring };
            vertices.push_back({ p, p, color });
        }
    }

    const XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
    for (int r = 0; r < last; ++r) {
        for (int s = 0; s < segments; ++s) {
            const int next = (s + 1) % segments;
            const int a = r * segments + s, b = r * segments + next;
            const int c = (r + 1) * segments + s, d = (r + 1) * segments + next;
            addOutward(indices, vertices, a, b, c, center);
            addOutward(indices, vertices, c, b, d, center);
        }
    }
    return createMesh(device, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(Vertex), indices);
}

// Estrella de cinco puntas extruida (radio 0.5, grosor 0.12), blanca: el color lo pone el tinte.
Renderer3D::Mesh Renderer3D::createStar(ID3D11Device* device) {
    constexpr int corners = 10; // 5 puntas y 5 huecos
    constexpr float outer = 0.5f, inner = 0.21f, half = 0.06f;
    const XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    const XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };

    XMFLOAT2 outline[corners];
    for (int i = 0; i < corners; ++i) {
        const float angle = XM_PIDIV2 + XM_2PI * static_cast<float>(i) / corners;
        const float radius = (i & 1) ? inner : outer;
        outline[i] = { std::cos(angle) * radius, std::sin(angle) * radius };
    }

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    // Caras delantera y trasera: abanico desde el centro.
    for (const float z : { half, -half }) {
        const XMFLOAT3 normal = { 0.0f, 0.0f, z > 0.0f ? 1.0f : -1.0f };
        const int first = static_cast<int>(vertices.size());
        vertices.push_back({ { 0.0f, 0.0f, z }, normal, white });
        for (const XMFLOAT2& p : outline) vertices.push_back({ { p.x, p.y, z }, normal, white });
        for (int i = 0; i < corners; ++i) addOutward(indices, vertices, first, first + 1 + i, first + 1 + (i + 1) % corners, center);
    }

    // Canto: un rectángulo por cada lado de la silueta.
    for (int i = 0; i < corners; ++i) {
        const XMFLOAT2& a = outline[i];
        const XMFLOAT2& b = outline[(i + 1) % corners];
        const float length = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
        const XMFLOAT3 normal = { (b.y - a.y) / length, -(b.x - a.x) / length, 0.0f };

        const int first = static_cast<int>(vertices.size());
        vertices.push_back({ { a.x, a.y, half }, normal, white });
        vertices.push_back({ { b.x, b.y, half }, normal, white });
        vertices.push_back({ { a.x, a.y, -half }, normal, white });
        vertices.push_back({ { b.x, b.y, -half }, normal, white });
        addOutward(indices, vertices, first, first + 1, first + 2, center);
        addOutward(indices, vertices, first + 2, first + 1, first + 3, center);
    }
    return createMesh(device, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(Vertex), indices);
}

// Rayo: cono abierto de 1 de largo a lo largo de +Z (radio 0.12 en z = 0, 1 en z = 1). col.r es la intensidad a lo largo del rayo.
Renderer3D::Mesh Renderer3D::createBeam(ID3D11Device* device) {
    constexpr int segments = 20;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    for (int ring = 0; ring < 2; ++ring) {
        const float z = static_cast<float>(ring), radius = ring == 0 ? 0.12f : 1.0f;
        const float shade = ring == 0 ? 1.0f : 0.35f;
        for (int s = 0; s < segments; ++s) {
            const float theta = static_cast<float>(s) / segments * XM_2PI;
            const float x = std::cos(theta), y = std::sin(theta);
            vertices.push_back({ { x * radius, y * radius, z }, { x, y, 0.0f }, { shade, shade, shade, 1.0f } });
        }
    }
    for (int s = 0; s < segments; ++s) {
        const int next = (s + 1) % segments;
        indices.insert(indices.end(), { static_cast<uint16_t>(s), static_cast<uint16_t>(next), static_cast<uint16_t>(segments + s),
                                        static_cast<uint16_t>(segments + s), static_cast<uint16_t>(next), static_cast<uint16_t>(segments + next) });
    }
    return createMesh(device, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(Vertex), indices);
}

void Renderer3D::init(ID3D11Device* device) {
    // 1. Shaders
    const ComPtr<ID3DBlob> vsBlob = D3dUtil::compileShader(kShaderCode, "VS", "vs_4_0");
    const ComPtr<ID3DBlob> psBlob = D3dUtil::compileShader(kShaderCode, "PS", "ps_4_0");
    D3dUtil::check(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader), "CreateVertexShader");
    D3dUtil::check(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader), "CreatePixelShader");

    const ComPtr<ID3DBlob> shadowVsBlob = D3dUtil::compileShader(kShaderCode, "VS_SHADOW", "vs_4_0");
    D3dUtil::check(device->CreateVertexShader(shadowVsBlob->GetBufferPointer(), shadowVsBlob->GetBufferSize(), nullptr, &m_shadowVertexShader), "CreateVertexShader (shadow)");

    const ComPtr<ID3DBlob> skyVsBlob = D3dUtil::compileShader(kShaderCode, "VS_SKY", "vs_4_0");
    const ComPtr<ID3DBlob> skyPsBlob = D3dUtil::compileShader(kShaderCode, "PS_SKY", "ps_4_0");
    D3dUtil::check(device->CreateVertexShader(skyVsBlob->GetBufferPointer(), skyVsBlob->GetBufferSize(), nullptr, &m_skyVertexShader), "CreateVertexShader (sky)");
    D3dUtil::check(device->CreatePixelShader(skyPsBlob->GetBufferPointer(), skyPsBlob->GetBufferSize(), nullptr, &m_skyPixelShader), "CreatePixelShader (sky)");

    const ComPtr<ID3DBlob> waterVsBlob = D3dUtil::compileShader(kShaderCode, "VS_WATER", "vs_4_0");
    const ComPtr<ID3DBlob> waterPsBlob = D3dUtil::compileShader(kShaderCode, "PS_WATER", "ps_4_0");
    D3dUtil::check(device->CreateVertexShader(waterVsBlob->GetBufferPointer(), waterVsBlob->GetBufferSize(), nullptr, &m_waterVertexShader), "CreateVertexShader (water)");
    D3dUtil::check(device->CreatePixelShader(waterPsBlob->GetBufferPointer(), waterPsBlob->GetBufferSize(), nullptr, &m_waterPixelShader), "CreatePixelShader (water)");

    const ComPtr<ID3DBlob> modelPsBlob = D3dUtil::compileShader(kShaderCode, "PS_MODEL", "ps_4_0");
    D3dUtil::check(device->CreatePixelShader(modelPsBlob->GetBufferPointer(), modelPsBlob->GetBufferSize(), nullptr, &m_modelPixelShader), "CreatePixelShader (model)");
    const ComPtr<ID3DBlob> glowPsBlob = D3dUtil::compileShader(kShaderCode, "PS_GLOW", "ps_4_0");
    D3dUtil::check(device->CreatePixelShader(glowPsBlob->GetBufferPointer(), glowPsBlob->GetBufferSize(), nullptr, &m_glowPixelShader), "CreatePixelShader (glow)");

    // 2. Input layout (el mapa de sombras reutiliza el de los objetos)
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                        D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3dUtil::check(device->CreateInputLayout(layout, _countof(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout), "CreateInputLayout");

    // 3. Buffers de constantes
    m_objectBuffer = D3dUtil::createBuffer(device, sizeof(ObjectConstants), D3D11_BIND_CONSTANT_BUFFER,
        nullptr, D3D11_USAGE_DEFAULT, "CreateBuffer (object constants)");
    m_frameBuffer = D3dUtil::createBuffer(device, sizeof(FrameConstants), D3D11_BIND_CONSTANT_BUFFER,
        nullptr, D3D11_USAGE_DEFAULT, "CreateBuffer (frame constants)");

    // 4. Estados: cielo sin profundidad y mapa de sombras (textura de profundidad legible desde el shader).
    D3D11_DEPTH_STENCIL_DESC skyDepth = {};
    skyDepth.DepthEnable = FALSE;
    skyDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyDepth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    D3dUtil::check(device->CreateDepthStencilState(&skyDepth, &m_skyDepth), "CreateDepthStencilState (sky)");

    // El agua: mezcla por alfa, prueba la profundidad sin escribirla y se ve por las dos caras.
    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    D3dUtil::check(device->CreateBlendState(&blend, &m_waterBlend), "CreateBlendState (water)");

    // El brillo suma su color a lo que hay detrás (sin escribir profundidad).
    D3D11_BLEND_DESC additive = blend;
    additive.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    additive.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    additive.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    additive.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    D3dUtil::check(device->CreateBlendState(&additive, &m_additiveBlend), "CreateBlendState (additive)");

    D3D11_SAMPLER_DESC modelSampler = {};
    modelSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    modelSampler.AddressU = modelSampler.AddressV = modelSampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    modelSampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    modelSampler.MaxLOD = D3D11_FLOAT32_MAX;
    D3dUtil::check(device->CreateSamplerState(&modelSampler, &m_modelSampler), "CreateSamplerState (model)");

    D3D11_DEPTH_STENCIL_DESC waterDepth = {};
    waterDepth.DepthEnable = TRUE;
    waterDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    waterDepth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    D3dUtil::check(device->CreateDepthStencilState(&waterDepth, &m_waterDepth), "CreateDepthStencilState (water)");

    D3D11_RASTERIZER_DESC noCull = {};
    noCull.FillMode = D3D11_FILL_SOLID;
    noCull.CullMode = D3D11_CULL_NONE;
    noCull.DepthClipEnable = TRUE;
    D3dUtil::check(device->CreateRasterizerState(&noCull, &m_noCull), "CreateRasterizerState (no cull)");

    D3D11_TEXTURE2D_DESC shadowDesc = {};
    shadowDesc.Width = kShadowMapSize;
    shadowDesc.Height = kShadowMapSize;
    shadowDesc.MipLevels = 1;
    shadowDesc.ArraySize = 1;
    shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    shadowDesc.SampleDesc.Count = 1;
    shadowDesc.Usage = D3D11_USAGE_DEFAULT;
    shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> shadowTexture;
    D3dUtil::check(device->CreateTexture2D(&shadowDesc, nullptr, &shadowTexture), "CreateTexture2D (shadow map)");

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    D3dUtil::check(device->CreateDepthStencilView(shadowTexture.Get(), &dsvDesc, &m_shadowDsv), "CreateDepthStencilView (shadow map)");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    D3dUtil::check(device->CreateShaderResourceView(shadowTexture.Get(), &srvDesc, &m_shadowSrv), "CreateShaderResourceView (shadow map)");

    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sampler.BorderColor[0] = sampler.BorderColor[1] = sampler.BorderColor[2] = sampler.BorderColor[3] = 1.0f; // fuera del mapa: iluminado
    sampler.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    D3dUtil::check(device->CreateSamplerState(&sampler, &m_shadowSampler), "CreateSamplerState (shadow)");

    D3D11_RASTERIZER_DESC raster = {};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_BACK;
    raster.DepthClipEnable = TRUE;
    raster.SlopeScaledDepthBias = 1.5f;
    D3dUtil::check(device->CreateRasterizerState(&raster, &m_shadowRaster), "CreateRasterizerState (shadow)");

    // 5. Geometría
    m_device = device;
    m_ocean = createOcean(device);
    m_cone = createCone(device);
    m_cube = createCube(device);
    m_sphere = createSphere(device);
    m_dome = createSphere(device, true);
    m_star = createStar(device);
    m_beam = createBeam(device);
    m_draws.reserve(256);
}

XMMATRIX Renderer3D::lightViewProj(const XMFLOAT3& lightDir, const XMFLOAT3& focus) {
    const XMVECTOR toLight = XMVector3Normalize(XMLoadFloat3(&lightDir));
    const XMMATRIX view = XMMatrixLookAtLH(XMVectorScale(toLight, kLightDistance), XMVectorZero(), g_XMIdentityR1);
    // El centro del volumen de sombras, en el espacio de la luz, salta de texel en texel: las sombras no tiemblan al moverse.
    XMFLOAT3 f;
    XMStoreFloat3(&f, XMVector3TransformCoord(XMLoadFloat3(&focus), view));
    const float texel = 2.0f * kShadowHalfExtent / static_cast<float>(kShadowMapSize);
    const float cx = std::floor(f.x / texel + 0.5f) * texel, cy = std::floor(f.y / texel + 0.5f) * texel;
    return view * XMMatrixOrthographicOffCenterLH(cx - kShadowHalfExtent, cx + kShadowHalfExtent, cy - kShadowHalfExtent, cy + kShadowHalfExtent,
                                                  f.z - kLightDepth * 0.5f, f.z + kLightDepth * 0.5f);
}

void Renderer3D::add(const Mesh& mesh, CXMMATRIX world, const XMFLOAT4& tint, bool castsShadow, DrawKind kind, ID3D11ShaderResourceView* texture) {
    Draw draw = { &mesh, {}, tint, castsShadow, kind, texture };
    XMStoreFloat4x4(&draw.world, world);
    m_draws.push_back(draw);
}

void Renderer3D::addModel(const RenderModel& model, CXMMATRIX world, const XMFLOAT4& tint, const ModelPose& pose, bool castsShadow) {
    for (size_t i = 0; i < model.parts.size(); ++i) {
        add(model.parts[i].mesh, model.partMatrix(static_cast<int>(i), pose.get(static_cast<int>(i))) * world, tint, castsShadow,
            DrawKind::MODEL, model.texture.Get());
    }
}

// Lista única de lo que hay en el mundo: la usan el mapa de sombras y la pantalla.
static const auto kStart = std::chrono::steady_clock::now(); // origen del reloj de adornos

void Renderer3D::collect(const Scene& scene) {
    m_draws.clear();

    for (const Physics::World::Box& wall : scene.world.obstacles) {
        add(m_cube, wallWorld(wall), kWallTint);
    }

    const XMFLOAT3& p = scene.player.body.position;
    addPlayer(scene);
    const float time = scene.dayCycle.lighting().time;
    m_clock = std::fmod(std::chrono::duration<float>(std::chrono::steady_clock::now() - kStart).count(), 3600.0f);
    addSky(scene, time);
    addGeysers(scene, time);
    addUnderground(scene);

    for (const CaptureTarget* wild : scene.wild.loaded()) {
        const CaptureTarget& target = *wild;
        const float size = target.scale();
        if (size > 0.001f) addCreature(target.drawCenter(), size, target.yaw(), target.speciesId(), target.shiny());

        if (const CaptureSequence* sequence = target.sequence()) addCapture(*sequence);
    }

    if (scene.companion.active()) {
        const Companion& pet = scene.companion;
        const float size = Companion::SIZE * pet.scale();
        if (size > 0.001f) {
            const XMFLOAT3 at = pet.drawCenter();
            addCreature({ at.x, at.y + pet.bob(), at.z }, size, pet.yaw(), pet.speciesId(), pet.shiny());
        }
        if (const BallTransfer* transfer = pet.transfer()) addTransfer(*transfer, pet.drawCenter(), Companion::SIZE * pet.scale());
    }

    for (const Scene::Strike& strike : scene.strikes) { // golpe del acompañante: rayo (a distancia) e impacto con el color de su tipo
        int r = 255, g = 255, b = 255;
        PokemonStyle::typeRgb(strike.typeId, r, g, b);
        const float fade = 1.0f - strike.age / Scene::STRIKE_TIME;
        const XMFLOAT3 tone = { static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f };
        if (strike.ranged) addGlowBetween(strike.from, strike.to, 0.16f * fade + 0.04f, { tone.x, tone.y, tone.z, 0.9f * fade });
        const float k = 0.25f + 0.6f * (1.0f - fade);
        add(m_sphere, XMMatrixScaling(k, k, k) * XMMatrixTranslation(strike.to.x, strike.to.y, strike.to.z), { tone.x, tone.y, tone.z, 1.1f * fade }, false, DrawKind::GLOW);
    }

    for (const Chest& chest : scene.chests.entities) addChest(chest);
    for (const ResourceNode& node : scene.nodes) {
        const float dx = node.body.position.x - p.x, dz = node.body.position.z - p.z;
        if (!node.gone() && dx * dx + dz * dz <= kNodeDrawRange * kNodeDrawRange) addNode(node);
    }
    for (const GroundItem& item : scene.groundItems.entities) addGroundItem(item);
    addMachine();
    addProfessor(scene);

    for (const Pokeball& ball : scene.balls) addBall(ball);
}

// Jugador: torso, cabeza, dos brazos y dos piernas (cubos) con la postura de PlayerPose (andar, trepar, nadar, bucear, usar una
// herramienta) y, si la usa, la herramienta en la mano.
void Renderer3D::addPlayer(const Scene& scene) {
    const Player& player = scene.player;
    const PlayerPose pose = player.pose();
    const XMFLOAT3& p = player.body.position;
    const XMMATRIX root = XMMatrixScaling(1.0f, player.heightScale(), 1.0f) * XMMatrixRotationY(player.facing()) * XMMatrixTranslation(p.x, p.y, p.z);
    const XMMATRIX hip = XMMatrixRotationX(pose.lean) * XMMatrixTranslation(0.0f, kHip, 0.0f) * root; // el cuerpo se inclina desde la cadera

    static const XMFLOAT4 skin = { 0.95f, 0.78f, 0.62f, 0.0f }, shirt = { 0.85f, 0.18f, 0.18f, 0.0f }, pants = { 0.20f, 0.26f, 0.52f, 0.0f }, hair = { 0.18f, 0.12f, 0.10f, 0.0f };
    constexpr float ARM = 0.5f, LEG = kHip, SHOULDER_X = 0.32f, SHOULDER_Y = 0.46f, LEG_X = 0.12f;

    add(m_cube, XMMatrixScaling(0.50f, 0.52f, 0.28f) * XMMatrixTranslation(0.0f, 0.26f, 0.0f) * hip, shirt);                  // torso
    add(m_cube, XMMatrixScaling(0.30f, 0.30f, 0.30f) * XMMatrixTranslation(0.0f, 0.68f, 0.02f) * hip, skin);                 // cabeza
    add(m_cube, XMMatrixScaling(0.32f, 0.10f, 0.32f) * XMMatrixTranslation(0.0f, 0.86f, 0.01f) * hip, hair);                 // pelo

    // Miembro colgando desde su articulación: 'angle' lo levanta hacia delante, 'roll' lo separa del cuerpo.
    const auto limbFrame = [&](float x, float y, float angle, float roll) {
        return XMMatrixRotationX(-angle) * XMMatrixRotationZ(roll) * XMMatrixTranslation(x, y, 0.0f) * hip;
    };
    const auto limb = [&](const XMFLOAT4& tint, float width, float length, const XMMATRIX& frame) {
        add(m_cube, XMMatrixScaling(width, length, width) * XMMatrixTranslation(0.0f, -length * 0.5f, 0.0f) * frame, tint);
    };
    const XMMATRIX rightArm = limbFrame(SHOULDER_X, SHOULDER_Y, pose.rightArm, pose.rightRoll);
    const XMMATRIX leftArm = limbFrame(-SHOULDER_X, SHOULDER_Y, pose.leftArm, -pose.leftRoll);
    limb(skin, 0.13f, ARM, rightArm);
    limb(skin, 0.13f, ARM, leftArm);
    limb(pants, 0.18f, LEG, limbFrame(LEG_X, 0.0f, pose.rightLeg, 0.0f));
    limb(pants, 0.18f, LEG, limbFrame(-LEG_X, 0.0f, pose.leftLeg, 0.0f));

    if (player.diving() && player.breathesUnderwater) { // burbuja de aire alrededor de la cabeza
        const float t = scene.world.time;
        add(m_cube, XMMatrixScaling(0.78f, 0.78f, 0.78f) * XMMatrixRotationY(t * 0.6f) * XMMatrixTranslation(0.0f, 0.68f, 0.02f) * hip, { 0.78f, 0.93f, 1.0f, 0.22f }, false);
        for (int i = 0; i < 6; ++i) {
            const float a = t * 1.4f + i * 1.0472f, rise = std::fmod(t * 0.7f + i * 0.17f, 1.0f);
            add(m_cube, XMMatrixScaling(0.07f, 0.07f, 0.07f) * XMMatrixTranslation(std::cos(a) * 0.42f, 0.35f + rise * 0.7f, std::sin(a) * 0.42f) * hip, { 0.9f, 0.97f, 1.0f, 0.45f }, false);
        }
    }

    if (pose.holding != PlayerAction::NONE) {
        const XMMATRIX hand = XMMatrixRotationX(-pose.toolTilt) * XMMatrixTranslation(0.0f, -ARM, 0.0f) * rightArm; // marco de la mano: -y sigue la dirección del brazo
        addTool(pose.holding, hand);
        if (pose.holding == PlayerAction::FISH) {
            XMFLOAT3 tip;
            XMStoreFloat3(&tip, XMVector3TransformCoord(XMVectorSet(0.0f, -ROD_LENGTH, 0.0f, 1.0f), hand));
            addFishingLine(scene, tip);
        }
    }
}

// La herramienta en la mano (marco: el origen es la mano y -y apunta hacia donde apunta el brazo).
void Renderer3D::addTool(PlayerAction tool, CXMMATRIX hand) {
    static const XMFLOAT4 wood = { 0.55f, 0.38f, 0.20f, 0.0f }, metal = { 0.72f, 0.74f, 0.78f, 0.0f }, blue = { 0.20f, 0.42f, 0.85f, 0.0f };
    const auto part = [&](const XMFLOAT4& tint, float sx, float sy, float sz, float x, float y, float z) {
        add(m_cube, XMMatrixScaling(sx, sy, sz) * XMMatrixTranslation(x, y, z) * hand, tint);
    };
    switch (tool) {
        case PlayerAction::CHOP:  // hacha: mango y hoja
            part(wood, 0.06f, 0.85f, 0.06f, 0.0f, -0.15f, 0.0f);
            part(metal, 0.05f, 0.22f, 0.30f, 0.0f, -0.52f, 0.13f);
            break;
        case PlayerAction::MINE:  // pico: mango y cabeza alargada
            part(wood, 0.06f, 0.85f, 0.06f, 0.0f, -0.15f, 0.0f);
            part(metal, 0.06f, 0.09f, 0.62f, 0.0f, -0.55f, 0.0f);
            break;
        case PlayerAction::WATER: // regadera: cuerpo y pitorro
            part(blue, 0.26f, 0.22f, 0.22f, 0.0f, -0.12f, 0.08f);
            part(blue, 0.05f, 0.05f, 0.30f, 0.0f, -0.10f, 0.30f);
            break;
        case PlayerAction::FISH:  // caña
            part(wood, 0.035f, ROD_LENGTH + 0.2f, 0.035f, 0.0f, -ROD_LENGTH * 0.5f + 0.1f, 0.0f);
            part(metal, 0.08f, 0.08f, 0.08f, 0.0f, -0.05f, 0.05f);
            break;
        default: break;
    }
}

// Hilo de la caña (de la punta al corcho) y el corcho: la mitad de arriba roja y la de abajo blanca; se hunde cuando pica.
void Renderer3D::addFishingLine(const Scene& scene, const XMFLOAT3& rodTip) {
    if (!scene.fishing.active()) return;
    XMFLOAT3 b = scene.fishing.bobber();
    const float t = scene.dayCycle.lighting().time;
    const float bite = scene.fishing.phase() == Fishing::Phase::BITING ? -0.12f + 0.05f * std::sin(t * 30.0f) : 0.0f;
    b.y += std::sin(t * 2.0f + b.x) * 0.03f + bite;

    const XMFLOAT3 d = { b.x - rodTip.x, b.y - rodTip.y, b.z - rodTip.z };
    const float length = (std::max)(std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z), 0.01f);
    const float yaw = std::atan2(d.x, d.z), pitch = -std::asin((std::clamp)(d.y / length, -1.0f, 1.0f));
    add(m_cube, XMMatrixScaling(0.012f, 0.012f, length) * XMMatrixRotationX(pitch) * XMMatrixRotationY(yaw) *
        XMMatrixTranslation((rodTip.x + b.x) * 0.5f, (rodTip.y + b.y) * 0.5f, (rodTip.z + b.z) * 0.5f), { 0.9f, 0.9f, 0.9f, 0.5f }, false);
    const float r = 0.12f;
    add(m_sphere, XMMatrixScaling(r, r, r) * XMMatrixTranslation(b.x, b.y + r * 0.4f, b.z), { 0.95f, 0.15f, 0.12f, 0.2f }, false);
}

// Decorado de las galerías cercanas a la cámara y las luces que iluminan sus paredes (lámparas y cristales).
void Renderer3D::addUnderground(const Scene& scene) {
    m_lamps.clear();
    const Underground& under = scene.habitats.under();
    if (under.empty()) return;
    const XMFLOAT3& eye = m_eye;
    const auto tint = [](float r, float g, float b, float glow = 0.0f) { return XMFLOAT4{ r, g, b, glow }; };
    const XMFLOAT4 rock = tint(0.95f, 0.93f, 0.9f), timber = tint(0.42f, 0.30f, 0.18f), steel = tint(0.55f, 0.56f, 0.62f);

    // Viga o barra entre dos puntos, de sección cuadrada.
    const auto bar = [&](const XMFLOAT3& a, const XMFLOAT3& b, float thickness, const XMFLOAT4& color) {
        const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length < 1.0e-3f) return;
        add(m_cube, XMMatrixScaling(thickness, thickness, length) * XMMatrixRotationRollPitchYaw(-std::asin(dy / length), std::atan2(dx, dz), 0.0f) *
                    XMMatrixTranslation((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f), color, false);
    };
    const auto block = [&](const XMFLOAT3& size, float yaw, const XMFLOAT3& at, const XMFLOAT4& color) {
        add(m_cube, XMMatrixScaling(size.x, size.y, size.z) * XMMatrixRotationY(yaw) * XMMatrixTranslation(at.x, at.y, at.z), color, false);
    };

    using Prop = Underground::Prop;
    for (const Prop& prop : under.props()) {
        const float dx = prop.a.x - eye.x, dz = prop.a.z - eye.z;
        const float d2 = dx * dx + dz * dz;
        if (d2 > kCaveDrawRange * kCaveDrawRange) continue;
        const Underground::Style& style = Underground::style(prop.color);
        const XMFLOAT3& a = prop.a;
        const float turn = std::fmod(a.x * 12.9898f + a.z * 78.233f, 6.2831853f); // giro fijo de cada elemento
        switch (prop.type) {
            case Prop::STALACTITE:
                add(m_cone, XMMatrixScaling(prop.w, prop.h, prop.w) * XMMatrixRotationY(turn) * XMMatrixTranslation(a.x, a.y, a.z), rock, false);
                break;
            case Prop::STALAGMITE:
                add(m_cone, XMMatrixScaling(prop.w, prop.h, prop.w) * XMMatrixRotationY(turn) * XMMatrixRotationX(XM_PI) * XMMatrixTranslation(a.x, a.y, a.z), rock, false);
                break;
            case Prop::PILLAR: {
                const float height = prop.b.y - a.y;
                add(m_cone, XMMatrixScaling(prop.w * 1.5f, height * 0.5f, prop.w * 1.5f) * XMMatrixTranslation(a.x, a.y + height * 0.5f, a.z), rock, false);
                add(m_cone, XMMatrixScaling(prop.w * 1.5f, height * 0.5f, prop.w * 1.5f) * XMMatrixRotationX(XM_PI) * XMMatrixTranslation(a.x, a.y + height * 0.5f, a.z), rock, false);
                break;
            }
            case Prop::BOULDER:
                block({ prop.w, prop.w * 0.7f, prop.w * 0.85f }, turn, { a.x, a.y, a.z }, tint(0.5f, 0.48f, 0.46f));
                break;
            case Prop::CRYSTAL:
                block({ prop.w, prop.h, prop.w }, turn, { a.x, a.y + prop.h * 0.5f, a.z }, tint(style.r, style.g, style.b, style.glow));
                block({ prop.w * 0.6f, prop.h * 0.7f, prop.w * 0.6f }, turn + 0.8f, { a.x + prop.w * 0.7f, a.y + prop.h * 0.35f, a.z }, tint(style.r, style.g, style.b, style.glow));
                if (d2 < kLampRange * kLampRange) m_lamps.push_back({ { a.x, a.y + prop.h * 0.5f, a.z }, 7.0f, { style.r * style.glow, style.g * style.glow, style.b * style.glow }, std::sqrt(d2) });
                break;
            case Prop::ORE:
                block({ prop.w, prop.w * 0.8f, prop.w }, turn, a, tint(0.34f, 0.32f, 0.31f));
                for (int k = 0; k < 3; ++k) {
                    const float ang = turn + k * 2.1f;
                    block({ prop.w * 0.32f, prop.w * 0.32f, prop.w * 0.32f }, ang, { a.x + std::cos(ang) * prop.w * 0.42f, a.y + (k - 1) * prop.w * 0.2f, a.z + std::sin(ang) * prop.w * 0.42f }, tint(style.r, style.g, style.b, style.glow));
                }
                break;
            case Prop::LAMP: {
                const XMFLOAT3& n = prop.b;
                const XMFLOAT3 tip = { a.x + n.x * 0.28f, a.y, a.z + n.z * 0.28f };
                bar(a, tip, 0.06f, timber);
                block({ 0.2f, 0.26f, 0.2f }, turn, { tip.x, tip.y - 0.08f, tip.z }, tint(1.0f, 0.78f, 0.35f, 1.0f));
                if (d2 < kLampRange * kLampRange) m_lamps.push_back({ tip, 13.0f, { 1.0f, 0.78f, 0.42f }, std::sqrt(d2) });
                break;
            }
            case Prop::POST:
                block({ prop.w, prop.h, prop.w }, 0.0f, { a.x, a.y + prop.h * 0.5f, a.z }, timber);
                break;
            case Prop::BEAM:
                bar(a, prop.b, prop.w, timber);
                break;
            case Prop::SLEEPER:
                block({ prop.w, 0.1f, 0.22f }, std::atan2(prop.b.x, prop.b.z), a, timber);
                break;
            case Prop::RAIL:
                bar(a, prop.b, prop.w, steel);
                break;
            case Prop::CART: {
                const float yaw = std::atan2(prop.b.x, prop.b.z), w = prop.w;
                block({ w * 0.9f, w * 0.5f, w * 1.3f }, yaw, { a.x, a.y + w * 0.5f, a.z }, tint(0.40f, 0.36f, 0.34f));
                block({ w * 0.75f, w * 0.18f, w * 1.1f }, yaw, { a.x, a.y + w * 0.78f, a.z }, tint(0.16f, 0.15f, 0.15f));
                const float cx = std::cos(yaw), sx = std::sin(yaw); // ruedas: dos por lado
                for (const float side : { -1.0f, 1.0f }) for (const float end : { -1.0f, 1.0f }) {
                    block({ 0.07f, w * 0.28f, w * 0.28f }, yaw, { a.x + cx * side * w * 0.5f + sx * end * w * 0.42f, a.y + w * 0.16f, a.z - sx * side * w * 0.5f + cx * end * w * 0.42f }, steel);
                }
                break;
            }
        }
    }
    std::sort(m_lamps.begin(), m_lamps.end(), [](const Lamp& l, const Lamp& r) { return l.distance < r.distance; });
    if (m_lamps.size() > static_cast<size_t>(kLamps)) m_lamps.resize(kLamps);
}

// Islas del cielo: en el mar, las corrientes ascendentes que llevan hasta ellas (la base de nube de cada isla es una malla fija: createSkyBase).
void Renderer3D::addSky(const Scene& scene, float time) {
    const XMFLOAT3& p = scene.player.body.position;
    for (const Physics::World::Updraft& stream : scene.habitats.updrafts()) { // la corriente: una espiral de bloques claros que sube del mar al cielo
        const float dx = stream.x - p.x, dz = stream.z - p.z;
        if (dx * dx + dz * dz > 300.0f * 300.0f) continue;
        const float base = (std::max)(scene.world.waterLevel(stream.x, stream.z), scene.world.groundHeight(stream.x, stream.z));
        const int count = static_cast<int>((stream.topY - base) / 3.0f);
        for (int i = 0; i < count; ++i) {
            const float a = time * 1.2f + i * 0.6f, r = stream.radius * (0.35f + 0.5f * std::fabs(std::sin(i * 1.3f)));
            add(m_cube, XMMatrixScaling(0.8f, 0.8f, 0.8f) * XMMatrixRotationY(a) * XMMatrixTranslation(stream.x + std::cos(a) * r, base + std::fmod(i * 3.0f + time * 14.0f, stream.topY - base), stream.z + std::sin(a) * r), { 0.82f, 0.95f, 1.0f, 1.0f }, false);
        }
    }
}

// Géiseres: el vapor sube siempre de la charca; al erupcionar, un chorro de bloques de agua blanca sube hasta su altura y cae.
void Renderer3D::addGeysers(const Scene& scene, float time) {
    const XMFLOAT3& p = scene.player.body.position;
    for (const Physics::World::Geyser& geyser : scene.world.geysers) {
        const float dx = geyser.x - p.x, dz = geyser.z - p.z;
        if (dx * dx + dz * dz > 220.0f * 220.0f) continue;
        const float base = (std::max)(scene.world.waterLevel(geyser.x, geyser.z), scene.world.groundHeight(geyser.x, geyser.z));
        for (int i = 0; i < 5; ++i) { // vapor
            const float rise = std::fmod(time * 0.8f + i * 0.2f, 1.0f), size = 0.5f + 1.2f * rise;
            add(m_cube, XMMatrixScaling(size, size, size) * XMMatrixRotationY(time + i) * XMMatrixTranslation(geyser.x + std::sin(i * 2.4f) * 0.5f, base + 0.4f + rise * 4.0f, geyser.z + std::cos(i * 2.4f) * 0.5f),
                { 0.92f, 0.94f, 0.96f, 1.0f }, false);
        }
        const float level = scene.world.eruption(geyser);
        if (level <= 0.0f) continue;
        const float top = geyser.height * level;
        const int count = static_cast<int>(top / 0.9f);
        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i) / (std::max)(count, 1), a = time * 9.0f + i * 1.9f, r = geyser.radius * 0.35f * (1.0f - 0.4f * t) * std::fabs(std::sin(i * 2.7f));
            const float s = 1.1f - 0.5f * t;
            add(m_cube, XMMatrixScaling(s, 0.9f, s) * XMMatrixRotationY(a) * XMMatrixTranslation(geyser.x + std::cos(a) * r, base + t * top, geyser.z + std::sin(a) * r),
                { 0.85f, 0.95f, 1.0f, 0.25f }, false);
        }
    }
}

// Pokémon (salvaje o compañero): cubo del color de su especie con un morro amarillo que marca hacia dónde mira.
void Renderer3D::addCreature(const XMFLOAT3& c, float size, float yaw, int speciesId, bool shiny) {
    const XMFLOAT3 base = PokemonStyle::color(speciesId);
    const XMFLOAT3 color = shiny ? PokemonStyle::shiny(base) : base;
    add(m_cube, XMMatrixScaling(size, size, size) * XMMatrixTranslation(c.x, c.y, c.z), { color.x, color.y, color.z, shiny ? 0.3f : 0.0f });

    const float nose = size * 0.4f;
    add(m_cube, XMMatrixScaling(nose, nose, nose) *
        XMMatrixTranslation(c.x + std::sin(yaw) * size * 0.5f, c.y + size * 0.15f, c.z + std::cos(yaw) * size * 0.5f), kNoseTint);

    if (!shiny) return;
    // Los shiny centellean: estrellas doradas que orbitan y laten a su alrededor.
    constexpr int SPARKLES = 5;
    constexpr XMFLOAT4 GOLD = { 1.0f, 0.92f, 0.45f, 1.0f };
    for (int i = 0; i < SPARKLES; ++i) {
        const float phase = m_clock * 1.4f + i * (XM_2PI / SPARKLES);
        const float pulse = 0.5f + 0.5f * std::sin(m_clock * 5.0f + i * 2.1f);
        const float star = size * (0.10f + 0.16f * pulse);
        const float radius = size * (0.85f + 0.2f * std::sin(m_clock * 0.9f + i));
        add(m_star, XMMatrixScaling(star, star, star) * XMMatrixRotationY(m_clock * 2.0f + i) *
            XMMatrixTranslation(c.x + std::cos(phase) * radius, c.y + size * (0.1f + 0.9f * std::fmod(m_clock * 0.35f + i * 0.2f, 1.0f)) - size * 0.3f, c.z + std::sin(phase) * radius), GOLD, false);
    }
}

// Nodo de recolección: árbol (tronco y copa), roca (bloques irregulares, con vetas si es mena) o arbusto. Se sacude al golpearlo.
// Nunca desaparece: un árbol talado deja el tocón y vuelve a crecer, una roca o mena regenera su mineral y un arbusto
// recogido se queda sin frutos hasta que le vuelven a crecer.
void Renderer3D::addNode(const ResourceNode& node) {
    const ResourceStyle::Look look = ResourceStyle::look(node.typeId());
    const XMFLOAT3& p = node.body.position;
    const float g = node.growth();
    const XMMATRIX base = XMMatrixTranslation(node.shakeOffset(), 0.0f, 0.0f) * XMMatrixRotationY(node.yaw()) * XMMatrixTranslation(p.x, p.y, p.z);

    // Bloque (tamaño, giro, posición local, color, brillo) bajo una transformación de grupo.
    const auto draw = [&](const XMMATRIX& group, const XMFLOAT3& size, float yaw, const XMFLOAT3& at, const XMFLOAT3& color, float glow) {
        add(m_cube, XMMatrixScaling(size.x, size.y, size.z) * XMMatrixRotationY(yaw) * XMMatrixTranslation(at.x, at.y, at.z) * group,
            { color.x, color.y, color.z, glow });
    };
    const auto part = [&](const XMFLOAT3& size, float yaw, const XMFLOAT3& at, const XMFLOAT3& color, float glow) { draw(base, size, yaw, at, color, glow); };

    using Shape = ResourceStyle::Shape;
    if (ResourceStyle::isTree(look.shape)) {
        using ResourceStyle::STUMP_HEIGHT;
        if (ResourceStyle::stump(look, g)) {
            part({ 0.55f, STUMP_HEIGHT, 0.55f }, 0.0f, { 0.0f, STUMP_HEIGHT * 0.5f, 0.0f }, look.body, 0.0f);
            part({ 0.4f, 0.03f, 0.4f }, 0.0f, { 0.0f, STUMP_HEIGHT + 0.01f, 0.0f }, { look.body.x * 1.35f, look.body.y * 1.3f, look.body.z * 1.2f }, 0.0f);
            return;
        }
        const float k = ResourceStyle::treeScale(g); // el árbol joven crece entero desde el tocón
        const XMMATRIX tree = XMMatrixScaling(k, k, k) * base;
        if (look.shape == Shape::CONIFER) {
            draw(tree, { 0.45f, 1.5f, 0.45f }, 0.0f, { 0.0f, 0.75f, 0.0f }, look.body, 0.0f);
            draw(tree, { 2.4f, 1.0f, 2.4f }, 0.0f, { 0.0f, 1.7f, 0.0f }, look.accent, 0.0f);
            draw(tree, { 1.7f, 1.0f, 1.7f }, 0.4f, { 0.0f, 2.55f, 0.0f }, look.accent, 0.0f);
            draw(tree, { 1.0f, 1.0f, 1.0f }, 0.8f, { 0.0f, 3.4f, 0.0f }, look.accent, 0.0f);
        } else if (look.shape == Shape::JUNGLE) {
            draw(tree, { 0.7f, 3.4f, 0.7f }, 0.0f, { 0.0f, 1.7f, 0.0f }, look.body, 0.0f);
            draw(tree, { 3.6f, 1.3f, 3.6f }, 0.0f, { 0.0f, 3.9f, 0.0f }, look.accent, 0.0f);
            draw(tree, { 2.6f, 1.2f, 2.6f }, 0.6f, { 0.3f, 4.9f, -0.2f }, { look.accent.x * 1.15f, look.accent.y * 1.15f, look.accent.z * 1.15f }, 0.0f);
            draw(tree, { 0.12f, 1.6f, 0.12f }, 0.0f, { 1.3f, 3.1f, 0.9f }, { 0.14f, 0.45f, 0.14f }, 0.0f); // lianas
            draw(tree, { 0.12f, 1.2f, 0.12f }, 0.0f, { -1.2f, 3.3f, -0.7f }, { 0.14f, 0.45f, 0.14f }, 0.0f);
        } else if (look.shape == Shape::ACACIA) {
            draw(tree, { 0.32f, 2.6f, 0.32f }, 0.1f, { 0.0f, 1.3f, 0.0f }, look.body, 0.0f);
            draw(tree, { 0.22f, 1.0f, 0.22f }, 0.0f, { 0.5f, 2.9f, 0.0f }, look.body, 0.0f);
            draw(tree, { 3.8f, 0.35f, 3.2f }, 0.3f, { 0.4f, 3.5f, 0.0f }, look.accent, 0.0f); // copa plana en sombrilla
            draw(tree, { 2.4f, 0.35f, 2.2f }, 0.9f, { 0.4f, 3.75f, 0.0f }, { look.accent.x * 1.1f, look.accent.y * 1.1f, look.accent.z * 1.1f }, 0.0f);
        } else if (look.shape == Shape::PALM) {
            draw(tree, { 0.35f, 2.9f, 0.35f }, 0.0f, { 0.0f, 1.45f, 0.0f }, look.body, 0.0f);
            draw(tree, { 2.6f, 0.2f, 0.6f }, 0.0f, { 0.0f, 3.0f, 0.0f }, look.accent, 0.0f);
            draw(tree, { 0.6f, 0.2f, 2.6f }, 0.0f, { 0.0f, 3.05f, 0.0f }, look.accent, 0.0f);
            draw(tree, { 2.2f, 0.2f, 0.5f }, 0.785f, { 0.0f, 3.1f, 0.0f }, look.accent, 0.0f);
            draw(tree, { 0.5f, 0.2f, 2.2f }, 0.785f, { 0.0f, 3.15f, 0.0f }, look.accent, 0.0f);
        } else {
            draw(tree, { 0.5f, 1.9f, 0.5f }, 0.0f, { 0.0f, 0.95f, 0.0f }, look.body, 0.0f);
            draw(tree, { 2.3f, 1.5f, 2.3f }, 0.0f, { 0.0f, 2.65f, 0.0f }, look.accent, 0.0f);
            draw(tree, { 1.5f, 1.2f, 1.5f }, 0.785f, { 0.0f, 3.65f, 0.0f }, look.accent, 0.0f);
        }
        return;
    }

    if (look.shape == Shape::FLOWER || look.shape == Shape::TULIP || look.shape == Shape::SUNFLOWER) { // una sola flor: tallo, hojas y cabeza que se abre al crecer
        const float k = std::clamp(0.35f + 0.65f * g, 0.0f, 1.0f);
        const float h = look.height * k * (look.shape == Shape::SUNFLOWER ? 0.9f : 0.75f);
        const float bloom = std::clamp((g - 0.25f) / 0.75f, 0.0f, 1.0f);
        const float stem = look.shape == Shape::SUNFLOWER ? 0.09f : 0.045f;
        part({ stem, h, stem }, 0.0f, { 0.0f, h * 0.5f, 0.0f }, look.body, 0.0f);
        part({ stem * 4.0f, 0.03f, stem * 1.6f }, 0.5f, { 0.07f, h * 0.35f, 0.0f }, look.body, 0.0f);   // hojas
        part({ stem * 1.6f, 0.03f, stem * 4.0f }, 0.2f, { -0.06f, h * 0.5f, 0.05f }, look.body, 0.0f);
        if (bloom <= 0.0f) return;
        if (look.shape == Shape::TULIP) { // cáliz cerrado de tres piezas
            part({ 0.13f * bloom, 0.12f * bloom, 0.13f * bloom }, 0.0f, { 0.0f, h + 0.04f, 0.0f }, look.accent, 0.15f);
            part({ 0.17f * bloom, 0.10f * bloom, 0.17f * bloom }, 0.785f, { 0.0f, h + 0.12f, 0.0f }, look.accent, 0.15f);
            part({ 0.08f * bloom, 0.06f * bloom, 0.08f * bloom }, 0.0f, { 0.0f, h + 0.2f, 0.0f }, { look.accent.x * 0.8f, look.accent.y * 0.8f, look.accent.z * 0.8f }, 0.1f);
        } else if (look.shape == Shape::SUNFLOWER) { // plato oscuro rodeado de pétalos amarillos, mirando de lado
            const float r = 0.5f * bloom;
            part({ r * 0.55f, r * 0.55f, 0.07f }, 0.0f, { 0.0f, h, 0.0f }, { 0.28f, 0.17f, 0.07f }, 0.0f);
            part({ r * 1.3f, r * 0.22f, 0.04f }, 0.0f, { 0.0f, h, -0.03f }, look.accent, 0.2f);
            part({ r * 0.22f, r * 1.3f, 0.04f }, 0.0f, { 0.0f, h, -0.03f }, look.accent, 0.2f);
            part({ r * 1.0f, r * 0.2f, 0.04f }, 0.0f, { 0.0f, h, -0.035f }, look.accent, 0.2f);
            part({ r * 0.2f, r * 1.0f, 0.04f }, 0.785f, { 0.0f, h, -0.035f }, look.accent, 0.2f);
        } else { // margarita / amapola: pétalos en cruz y centro amarillo
            const float s = 0.30f * bloom;
            part({ s, 0.03f, s * 0.4f }, 0.0f, { 0.0f, h + 0.02f, 0.0f }, look.accent, 0.15f);
            part({ s * 0.4f, 0.03f, s }, 0.0f, { 0.0f, h + 0.025f, 0.0f }, look.accent, 0.15f);
            part({ s * 0.8f, 0.03f, s * 0.35f }, 0.785f, { 0.0f, h + 0.022f, 0.0f }, look.accent, 0.15f);
            part({ s * 0.35f, 0.03f, s * 0.8f }, 0.785f, { 0.0f, h + 0.028f, 0.0f }, look.accent, 0.15f);
            part({ s * 0.38f, 0.05f, s * 0.38f }, 0.0f, { 0.0f, h + 0.045f, 0.0f }, look.shape == Shape::FLOWER && look.accent.x > 0.9f && look.accent.y > 0.9f ? XMFLOAT3{ 0.99f, 0.85f, 0.22f } : XMFLOAT3{ 0.12f, 0.08f, 0.06f }, 0.2f);
        }
        return;
    }

    if (look.shape == Shape::CORAL) { // ramas de coral de colores sobre una base
        const float k = std::clamp(0.3f + 0.7f * g, 0.0f, 1.0f);
        part({ 0.9f * k, 0.25f * k, 0.9f * k }, 0.3f, { 0.0f, 0.12f * k, 0.0f }, look.body, 0.0f);
        static const float AT[][3] = { { 0.0f, 0.0f, 1.0f }, { 0.3f, 0.2f, 0.7f }, { -0.3f, 0.15f, 0.8f }, { 0.1f, -0.3f, 0.6f }, { -0.25f, -0.2f, 0.5f } };
        for (int i = 0; i < 5; ++i) {
            const float h = 0.75f * AT[i][2] * k;
            const XMFLOAT3& c = (i % 2) ? look.accent : look.body;
            part({ 0.16f, h, 0.16f }, 0.0f, { AT[i][0], 0.2f + h * 0.5f, AT[i][1] }, c, 0.1f);
            part({ 0.11f, h * 0.45f, 0.11f }, 0.7f, { AT[i][0] + 0.1f, 0.2f + h * 0.85f, AT[i][1] }, c, 0.1f);
        }
        return;
    }

    if (look.shape == Shape::KELP) { // frondas largas que ondulan con la corriente
        const float k = std::clamp(0.3f + 0.7f * g, 0.0f, 1.0f);
        static const float AT[][2] = { { 0.0f, 0.0f }, { 0.3f, 0.15f }, { -0.25f, 0.2f }, { 0.1f, -0.3f } };
        for (int i = 0; i < 4; ++i) {
            const float h = look.height * k * (0.7f + 0.1f * i);
            constexpr int SEGMENTS = 4;
            for (int j = 0; j < SEGMENTS; ++j) {
                const float t = static_cast<float>(j) / SEGMENTS, sway = 0.18f * t * std::sin(m_clock * 1.3f + i * 1.7f + t * 2.0f);
                part({ 0.09f, h / SEGMENTS, 0.05f }, 0.0f, { AT[i][0] + sway, h * (t + 0.5f / SEGMENTS), AT[i][1] }, (j % 2) ? look.accent : look.body, 0.0f);
            }
        }
        return;
    }

    if (look.shape == Shape::BUSH) {
        part({ 1.0f, 0.6f, 1.0f }, 0.3f, { 0.0f, 0.3f, 0.0f }, look.body, 0.0f);
        part({ 0.7f, 0.5f, 0.7f }, 0.9f, { 0.15f, 0.65f, -0.1f }, look.body, 0.0f);
        const float fruit = std::clamp((g - 0.35f) / 0.65f, 0.0f, 1.0f); // los frutos van saliendo y creciendo
        if (fruit > 0.0f) {
            const float f = 0.2f * fruit;
            part({ f, f, f }, 0.4f, { 0.45f, 0.45f, 0.2f }, look.accent, 0.3f);
            part({ f, f, f }, 0.2f, { -0.3f, 0.5f, 0.4f }, look.accent, 0.3f);
            part({ f, f, f }, 0.7f, { 0.2f, 0.85f, 0.3f }, look.accent, 0.3f);
            part({ f, f, f }, 0.1f, { -0.4f, 0.35f, -0.3f }, look.accent, 0.3f);
        }
        return;
    }

    // Roca: el cuerpo es una roca común (que se regenera entera) o la base de una mena (que regenera sus vetas).
    const float body = ResourceStyle::rockScale(look, g);
    const XMMATRIX rock = XMMatrixScaling(body, body, body) * base;
    draw(rock, { 1.6f, 0.95f, 1.4f }, 0.4f, { 0.0f, 0.475f, 0.0f }, look.body, 0.0f);
    draw(rock, { 0.9f, 0.75f, 0.9f }, 1.1f, { 0.5f, 0.375f, -0.3f }, look.body, 0.0f);
    draw(rock, { 0.7f, 0.6f, 0.7f }, 0.3f, { -0.55f, 0.3f, 0.4f }, look.body, 0.0f);
    if (look.specks && g > 0.05f) {
        const float v = g; // vetas: crecen con el avance
        part({ 0.3f * v, 0.3f * v, 0.3f * v }, 0.6f, { 0.2f, 0.95f, 0.1f }, look.accent, 0.35f);
        part({ 0.28f * v, 0.28f * v, 0.28f * v }, 0.2f, { 0.85f, 0.45f, 0.1f }, look.accent, 0.35f);
        part({ 0.3f * v, 0.3f * v, 0.3f * v }, 0.9f, { -0.15f, 0.5f, 0.7f }, look.accent, 0.35f);
        part({ 0.26f * v, 0.26f * v, 0.26f * v }, 0.4f, { 0.5f, 0.75f, -0.3f }, look.accent, 0.35f);
    }
}

// Objeto suelto: semiesfera blanca que brilla y late. No proyecta sombra.
void Renderer3D::addGroundItem(const GroundItem& item) {
    const XMFLOAT3& p = item.body.position;
    const float k = item.scale();
    if (k <= 0.001f) return;

    const float r = GroundItem::RADIUS * k;
    add(m_dome, XMMatrixScaling(r, r, r) * XMMatrixTranslation(p.x, p.y, p.z), { 1.0f, 1.0f, 1.0f, item.glow() }, false);
}

// Máquina de investigación: cuerpo metálico con una pantalla que brilla, un panel y una antena.
// Profesor Oak: bata blanca, pantalón oscuro y pelo canoso; se vuelve hacia el jugador.
void Renderer3D::addProfessor(const Scene& scene) {
    const XMFLOAT3& p = Professor::POSITION;
    const XMFLOAT3& you = scene.player.body.position;
    const XMMATRIX place = XMMatrixRotationY(Professor::yawToward(you.x, you.z)) * XMMatrixTranslation(p.x, p.y, p.z);
    const auto part = [&](const XMFLOAT3& size, const XMFLOAT3& at, const XMFLOAT3& color) {
        add(m_cube, XMMatrixScaling(size.x, size.y, size.z) * XMMatrixTranslation(at.x, at.y, at.z) * place, { color.x, color.y, color.z, 0.0f });
    };
    const XMFLOAT3 coat = { 0.93f, 0.93f, 0.95f }, skin = { 0.93f, 0.76f, 0.60f }, hair = { 0.62f, 0.60f, 0.58f }, pants = { 0.30f, 0.27f, 0.24f }, shirt = { 0.40f, 0.30f, 0.55f };
    part({ 0.17f, 0.80f, 0.20f }, { 0.12f, 0.40f, 0.0f }, pants);                  // piernas
    part({ 0.17f, 0.80f, 0.20f }, { -0.12f, 0.40f, 0.0f }, pants);
    part({ 0.52f, 0.70f, 0.30f }, { 0.0f, 1.15f, 0.0f }, coat);                    // bata
    part({ 0.20f, 0.60f, 0.05f }, { 0.0f, 1.20f, 0.16f }, shirt);                  // camisa bajo la bata
    part({ 0.14f, 0.62f, 0.16f }, { 0.34f, 1.12f, 0.0f }, coat);                   // brazos
    part({ 0.14f, 0.62f, 0.16f }, { -0.34f, 1.12f, 0.0f }, coat);
    part({ 0.30f, 0.30f, 0.30f }, { 0.0f, 1.65f, 0.0f }, skin);                    // cabeza
    part({ 0.33f, 0.12f, 0.33f }, { 0.0f, 1.83f, -0.02f }, hair);                  // pelo
}

void Renderer3D::addMachine() {
    const XMMATRIX place = XMMatrixRotationY(ResearchMachine::YAW) *
                           XMMatrixTranslation(ResearchMachine::POSITION.x, ResearchMachine::POSITION.y, ResearchMachine::POSITION.z);
    const auto part = [&](const XMFLOAT3& size, const XMFLOAT3& at, const XMFLOAT3& color, float glow) {
        add(m_cube, XMMatrixScaling(size.x, size.y, size.z) * XMMatrixTranslation(at.x, at.y, at.z) * place, { color.x, color.y, color.z, glow });
    };
    constexpr float W = ResearchMachine::WIDTH, D = ResearchMachine::DEPTH, H = ResearchMachine::HEIGHT;
    part({ W, H * 0.62f, D }, { 0.0f, H * 0.31f, 0.0f }, { 0.34f, 0.38f, 0.46f }, 0.0f);                 // cuerpo
    part({ W * 0.9f, H * 0.34f, D * 0.8f }, { 0.0f, H * 0.62f + H * 0.17f, -D * 0.05f }, { 0.26f, 0.29f, 0.36f }, 0.0f); // cabezal
    part({ W * 0.7f, H * 0.2f, 0.06f }, { 0.0f, H * 0.8f, D * 0.36f }, { 0.35f, 0.85f, 0.95f }, 0.9f);   // pantalla
    part({ W * 0.6f, 0.08f, 0.06f }, { 0.0f, H * 0.45f, D * 0.5f }, { 0.95f, 0.78f, 0.22f }, 0.5f);      // franja de luz
    part({ 0.08f, 0.5f, 0.08f }, { W * 0.3f, H + 0.25f, 0.0f }, { 0.7f, 0.7f, 0.75f }, 0.0f);            // antena
}

// Cofre: cuerpo, cerradura y tapa con bisagra atrás; al abrirse suelta chispas del color de su rareza.
void Renderer3D::addChest(const Chest& chest) {
    const ChestStyle::Look look = ChestStyle::look(chest.rarity());
    const float baseH = Chest::HEIGHT * Chest::BASE_RATIO;
    const float lidH = Chest::HEIGHT - baseH;
    const XMFLOAT3& p = chest.body.position;
    const float s = chest.scale();
    const XMMATRIX place = XMMatrixScaling(s, s, s) * XMMatrixRotationY(chest.yaw()) * XMMatrixTranslation(p.x, p.y, p.z);

    add(m_cube, XMMatrixScaling(Chest::WIDTH, baseH, Chest::DEPTH) * XMMatrixTranslation(0.0f, baseH * 0.5f, 0.0f) * place,
        { ChestStyle::BODY.x, ChestStyle::BODY.y, ChestStyle::BODY.z, 0.0f });
    add(m_cube, XMMatrixScaling(0.16f, 0.2f, 0.06f) * XMMatrixTranslation(0.0f, baseH, Chest::DEPTH * 0.5f) * place,
        { ChestStyle::LOCK.x, ChestStyle::LOCK.y, ChestStyle::LOCK.z, 0.0f });

    // La tapa gira alrededor de su borde trasero superior (bisagra).
    add(m_cube, XMMatrixScaling(Chest::WIDTH, lidH, Chest::DEPTH) * XMMatrixTranslation(0.0f, lidH * 0.5f, Chest::DEPTH * 0.5f) *
        XMMatrixRotationX(-chest.lidAngle()) * XMMatrixTranslation(0.0f, baseH, -Chest::DEPTH * 0.5f) * place,
        { look.lid.x, look.lid.y, look.lid.z, look.glow });

    for (int i = 0; i < look.sparkles; ++i) {
        XMFLOAT3 position;
        float size, spin;
        chest.sparkle(i, look.sparkles, position, size, spin);
        if (size > 0.001f) {
            add(m_star, XMMatrixScaling(size, size, size) * XMMatrixRotationY(spin) * XMMatrixTranslation(position.x, position.y, position.z),
                { look.lid.x * 0.5f + 0.5f, look.lid.y * 0.5f + 0.5f, look.lid.z * 0.5f + 0.5f, 1.0f }, false);
        }
    }
}

// Modelo de la pokéball de un tipo: el de su aspecto (PokeballStyle::look), o el de la Poké Ball si falta el archivo. Se carga una vez.
const Renderer3D::BallModel& Renderer3D::ballModel(int ballType) {
    const PokeballStyle::Look& look = PokeballStyle::look(ballType);
    const auto found = m_ballModels.find(look.model);
    if (found != m_ballModels.end()) return found->second;
    BallModel entry;
    entry.model = m_models.get(m_device, look.model, look.tintable);
    if (!entry.model && std::string(look.model) != PokeballStyle::DEFAULT_MODEL) return ballModel(-1); // sin su archivo, la de siempre
    entry.lid = entry.model ? entry.model->part(PokeballStyle::LID_PART) : -1;
    return m_ballModels[look.model] = entry;
}

// Pokéball del tipo dado: su modelo .glb (o una esfera si no se pudo cargar), centrado en 'position', del tamaño de la física, orientado
// según yaw (hacia dónde mira el botón), spin (rodando o inclinada), roll (giro sobre el eje del botón) y con la tapa abierta 'lid' (0..1).
void Renderer3D::addBall(int ballType, const XMFLOAT3& position, float yaw, float spin, float roll, float lid, float scale, float glow) {
    if (scale <= 0.001f) return;
    const PokeballStyle::Look& look = PokeballStyle::look(ballType);
    const XMFLOAT3& color = look.color;
    const XMMATRIX orient = XMMatrixRotationZ(roll) * XMMatrixRotationX(spin) * XMMatrixRotationY(yaw + look.frontYaw) * XMMatrixTranslation(position.x, position.y, position.z);
    const BallModel& ball = ballModel(ballType);
    if (!ball.model) {
        const float r = Pokeball::RADIUS * scale;
        add(m_sphere, XMMatrixScaling(r, r, r) * orient, { color.x, color.y, color.z, glow });
        return;
    }
    const XMFLOAT3 c = ball.model->center();
    const float s = Pokeball::RADIUS * 2.0f * scale / ball.model->height();
    ModelPose pose;
    pose.set(ball.lid, lid);
    const XMFLOAT4 tint = look.tintable ? XMFLOAT4{ color.x, color.y, color.z, glow } : XMFLOAT4{ 1.0f, 1.0f, 1.0f, glow };
    addModel(*ball.model, XMMatrixTranslation(-c.x, -c.y, -c.z) * XMMatrixScaling(s, s, s) * orient, tint, pose);
}

// Sacar o guardar a un pokémon: la bola con su tapa, el rayo rojo y la silueta roja del pokémon mientras entra o sale.
void Renderer3D::addTransfer(const BallTransfer& transfer, const XMFLOAT3& creature, float creatureSize) {
    static const XMFLOAT3 red = { 1.0f, 0.16f, 0.10f };
    const BallTransfer::Pose pose = transfer.pose();
    addBall(transfer.ballType(), pose.position, pose.yaw, 0.0f, 0.0f, pose.lid, pose.scale, pose.glow);
    const BallTransfer::Beam beam = transfer.beam(creature, creatureSize);
    if (!beam.active || beam.intensity <= 0.01f) return;
    addGlowBetween(beam.from, beam.to, beam.width, { red.x, red.y, red.z, 0.9f * beam.intensity });
    const float k = creatureSize * 1.05f;
    add(m_cube, XMMatrixScaling(k, k, k) * XMMatrixTranslation(creature.x, creature.y, creature.z), { red.x, red.y, red.z, 1.0f * beam.intensity }, false, DrawKind::GLOW);
}

// Segmento entre dos puntos como cubo largo y fino orientado (el hilo de la caña) o, con GLOW, el rayo de la captura.
void Renderer3D::addGlowBetween(const XMFLOAT3& from, const XMFLOAT3& to, float width, const XMFLOAT4& tint) {
    const XMFLOAT3 d = { to.x - from.x, to.y - from.y, to.z - from.z };
    const float length = (std::max)(std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z), 0.01f);
    const float yaw = std::atan2(d.x, d.z), pitch = -std::asin((std::clamp)(d.y / length, -1.0f, 1.0f));
    add(m_beam, XMMatrixScaling(width, width, length) * XMMatrixRotationX(pitch) * XMMatrixRotationY(yaw) * XMMatrixTranslation(from.x, from.y, from.z), tint, false, DrawKind::GLOW);
}

// Todo lo que se ve de una captura: la bola con su tapa, el rayo rojo, la silueta del pokémon, destellos y partículas.
void Renderer3D::addCapture(const CaptureSequence& sequence) {
    const CaptureSequence::BallPose pose = sequence.ballPose();
    addBall(sequence.ball().typeId, pose.position, pose.yaw, pose.spin, pose.roll, pose.lid, pose.scale, pose.glow);

    static const XMFLOAT3 red = { 1.0f, 0.16f, 0.10f };
    const CaptureSequence::Beam beam = sequence.beam();
    if (beam.active && beam.intensity > 0.01f) addGlowBetween(beam.from, beam.to, beam.width, { red.x, red.y, red.z, 0.9f * beam.intensity });
    const CaptureSequence::Silhouette silhouette = sequence.silhouette();
    if (silhouette.active && silhouette.size > 0.01f) {
        const float k = silhouette.size;
        add(m_cube, XMMatrixScaling(k, k, k) * XMMatrixTranslation(silhouette.center.x, silhouette.center.y, silhouette.center.z),
            { red.x, red.y, red.z, 1.1f * silhouette.intensity }, false, DrawKind::GLOW);
    }
    const CaptureSequence::Flash flash = sequence.flash();
    if (flash.active) {
        const float r = flash.radius;
        add(m_sphere, XMMatrixScaling(r, r, r) * XMMatrixTranslation(flash.center.x, flash.center.y, flash.center.z),
            { flash.color.x, flash.color.y, flash.color.z, 1.4f * flash.intensity }, false, DrawKind::GLOW);
    }
    for (const Anim::Particle& p : sequence.particles().particles()) {
        const float size = p.size();
        if (size <= 0.001f) continue;
        const XMFLOAT3 color = p.color();
        const XMMATRIX place = XMMatrixTranslation(p.position.x, p.position.y, p.position.z);
        if (p.shape == Anim::Shape::STAR) add(m_star, XMMatrixScaling(size, size, size) * XMMatrixRotationY(p.angle) * place, { color.x, color.y, color.z, 1.0f }, false);
        else add(m_cube, XMMatrixScaling(size, size, size) * XMMatrixRotationY(p.angle) * XMMatrixRotationX(p.angle * 0.7f) * place, { color.x, color.y, color.z, 1.0f }, false);
    }
}

void Renderer3D::setObject(ID3D11DeviceContext* context, CXMMATRIX world, CXMMATRIX worldViewProj, const XMFLOAT4& tint) const {
    ObjectConstants cb;
    XMStoreFloat4x4(&cb.worldViewProj, XMMatrixTranspose(worldViewProj));
    XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));
    cb.tint = tint;
    context->UpdateSubresource(m_objectBuffer.Get(), 0, nullptr, &cb, 0, 0);
}

void Renderer3D::drawIndexed(ID3D11DeviceContext* context, const Mesh& mesh) const {
    ID3D11Buffer* vb = mesh.vertices.Get();
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vb, &mesh.stride, &offset);
    context->IASetIndexBuffer(mesh.indices.Get(), mesh.indexFormat, 0);
    context->DrawIndexed(mesh.indexCount, 0, 0);
}

// Dibuja todos los objetos del mundo con la cámara dada (la de la luz o la del jugador).
void Renderer3D::drawAll(ID3D11DeviceContext* context, CXMMATRIX viewProj, bool shadowPass) const {
    DrawKind bound = DrawKind::SOLID; // shader de píxeles activo (la pasada de sombras no usa ninguno)
    for (const Draw& draw : m_draws) {
        if (draw.kind == DrawKind::GLOW) continue;
        if (shadowPass) {
            if (!draw.castsShadow) continue;
        } else if (draw.kind != bound) {
            context->PSSetShader(draw.kind == DrawKind::MODEL ? m_modelPixelShader.Get() : m_pixelShader.Get(), nullptr, 0);
            bound = draw.kind;
        }
        if (!shadowPass && draw.kind == DrawKind::MODEL) {
            ID3D11ShaderResourceView* texture = draw.texture;
            context->PSSetShaderResources(1, 1, &texture);
        }
        const XMMATRIX world = XMLoadFloat4x4(&draw.world);
        setObject(context, world, world * viewProj, draw.tint);
        drawIndexed(context, *draw.mesh);
    }
    if (!shadowPass && bound != DrawKind::SOLID) context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
}

// Rayos, siluetas y destellos: se suman al color de lo que hay detrás.
void Renderer3D::drawGlow(ID3D11DeviceContext* context, CXMMATRIX viewProj) const {
    const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(m_additiveBlend.Get(), blendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_waterDepth.Get(), 0);
    context->RSSetState(m_noCull.Get());
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_glowPixelShader.Get(), nullptr, 0);
    for (const Draw& draw : m_draws) {
        if (draw.kind != DrawKind::GLOW) continue;
        const XMMATRIX world = XMLoadFloat4x4(&draw.world);
        setObject(context, world, world * viewProj, draw.tint);
        drawIndexed(context, *draw.mesh);
    }
    context->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(nullptr, 0);
    context->RSSetState(nullptr);
}

// Pasada de profundidad desde la luz. El suelo no proyecta sombra (solo la recibe).
void Renderer3D::renderShadowMap(ID3D11DeviceContext* context, CXMMATRIX lightViewProj) const {
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11DepthStencilView> dsv;
    context->OMGetRenderTargets(1, rtv.GetAddressOf(), dsv.GetAddressOf());
    UINT viewportCount = 1;
    D3D11_VIEWPORT viewport = {};
    context->RSGetViewports(&viewportCount, &viewport);

    ID3D11ShaderResourceView* none = nullptr;
    context->PSSetShaderResources(0, 1, &none); // el mapa no puede estar a la vez como entrada y como destino

    const D3D11_VIEWPORT shadowViewport = { 0.0f, 0.0f, static_cast<float>(kShadowMapSize), static_cast<float>(kShadowMapSize), 0.0f, 1.0f };
    context->OMSetRenderTargets(0, nullptr, m_shadowDsv.Get());
    context->ClearDepthStencilView(m_shadowDsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    context->RSSetViewports(1, &shadowViewport);
    context->RSSetState(m_shadowRaster.Get());

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_shadowVertexShader.Get(), nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    drawAll(context, lightViewProj, true);

    context->RSSetState(nullptr);
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
}

// Dibuja los trozos de malla cuya esfera envolvente cae a menos de kTerrainDrawRange de la cámara.
void Renderer3D::drawChunks(ID3D11DeviceContext* context, const std::vector<Chunk>& chunks, CXMMATRIX viewProj, float range) const {
    setObject(context, XMMatrixIdentity(), viewProj, kWhite);
    for (const Chunk& chunk : chunks) {
        const float dx = chunk.center.x - m_eye.x, dy = chunk.center.y - m_eye.y, dz = chunk.center.z - m_eye.z;
        const float reach = range + chunk.radius;
        if (dx * dx + dy * dy + dz * dz > reach * reach) continue;
        drawIndexed(context, chunk.mesh);
    }
}

// El agua translúcida, después de todo lo opaco: se ve lo que hay debajo según su profundidad.
void Renderer3D::drawWater(ID3D11DeviceContext* context, CXMMATRIX viewProj) const {
    const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(m_waterBlend.Get(), blendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_waterDepth.Get(), 0);
    context->RSSetState(m_noCull.Get());
    context->VSSetShader(m_waterVertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_waterPixelShader.Get(), nullptr, 0);

    drawChunks(context, m_water, viewProj, kTerrainDrawRange);
    drawChunks(context, m_caveWater, viewProj, kCaveDrawRange);
    drawIndexed(context, m_ocean);

    context->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(nullptr, 0);
    context->RSSetState(nullptr);
}

void Renderer3D::updateFrame(ID3D11DeviceContext* context, const DayCycle::Lighting& light, CXMMATRIX view, CXMMATRIX lightViewProj, const Environment& env) {
    XMMATRIX viewRotation = view;
    viewRotation.r[3] = g_XMIdentityR3; // el cielo no se mueve con la posición de la cámara
    const XMMATRIX inverse = XMMatrixInverse(nullptr, viewRotation * XMLoadFloat4x4(&m_proj));

    FrameConstants fc;
    XMStoreFloat4x4(&fc.invSky, XMMatrixTranspose(inverse));
    XMStoreFloat4x4(&fc.lightViewProj, XMMatrixTranspose(lightViewProj));
    fc.lightDir = { light.lightDir.x, light.lightDir.y, light.lightDir.z, 0.0f };
    fc.lightColor = { light.lightColor.x, light.lightColor.y, light.lightColor.z, 0.0f };
    fc.ambientSky = { light.ambientSky.x, light.ambientSky.y, light.ambientSky.z, 0.0f };
    fc.ambientGround = { light.ambientGround.x, light.ambientGround.y, light.ambientGround.z, 0.0f };
    fc.sunDir = { light.sunDir.x, light.sunDir.y, light.sunDir.z, 0.0f };
    fc.moonDir = { light.moonDir.x, light.moonDir.y, light.moonDir.z, 0.0f };
    fc.skyZenith = { light.skyZenith.x, light.skyZenith.y, light.skyZenith.z, 0.0f };
    fc.skyHorizon = { light.skyHorizon.x, light.skyHorizon.y, light.skyHorizon.z, 0.0f };
    fc.skyParams = { light.starVisibility, light.warm, light.angle, light.time };
    fc.shadowParams = { 1.0f / static_cast<float>(kShadowMapSize), kShadowNormalOffset, kShadowDepthBias, 0.0f };
    fc.cameraPos = { env.eye.x, env.eye.y, env.eye.z, env.tide };
    const float day = (std::min)(1.0f, 0.25f + light.ambientSky.y * 1.2f); // el agua turbia y la niebla se oscurecen de noche
    const XMFLOAT3 fog = env.underwater ? XMFLOAT3{ 0.04f * day, 0.22f * day, 0.36f * day } : XMFLOAT3{ light.skyHorizon.x, light.skyHorizon.y, light.skyHorizon.z };
    fc.env = { env.darkness, env.underwater ? kWaterFogDensity : (env.darkness > 0.5f ? 0.0f : kFogDensity), env.underwater ? 1.0f : 0.0f, env.worldRadius };
    fc.fogColor = { fog.x * (1.0f - env.darkness), fog.y * (1.0f - env.darkness), fog.z * (1.0f - env.darkness), 0.0f };
    for (int i = 0; i < kLamps; ++i) {
        const bool used = i < static_cast<int>(m_lamps.size());
        fc.lampPos[i] = used ? XMFLOAT4{ m_lamps[i].position.x, m_lamps[i].position.y, m_lamps[i].position.z, m_lamps[i].reach } : XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };
        fc.lampColor[i] = used ? XMFLOAT4{ m_lamps[i].color.x, m_lamps[i].color.y, m_lamps[i].color.z, 0.0f } : XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };
    }
    context->UpdateSubresource(m_frameBuffer.Get(), 0, nullptr, &fc, 0, 0);
}

void Renderer3D::render(ID3D11DeviceContext* context, const Scene& scene, int width, int height) {
    if (width <= 0 || height <= 0) return;

    if (!m_worldBuilt || m_worldSeed != scene.habitats.seed()) {
        m_terrain = createTerrain(m_device, scene.habitats, scene.world, scene.data());
        m_water = createWater(m_device, scene.habitats, scene.world);
        m_cave = createUnderground(m_device, scene.habitats, scene.data());
        m_caveWater = createCaveWater(m_device, scene.habitats);
        m_skyBase = createSkyBase(m_device, scene.habitats);
        m_worldSeed = scene.habitats.seed();
        m_worldBuilt = true;
    }

    const float aspect = static_cast<float>(width) / height;
    const float fov = scene.camera.fov();
    if (aspect != m_aspect || fov != m_fov) {
        m_aspect = aspect;
        m_fov = fov;
        XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(fov, aspect, 0.1f, kFarPlane));
    }

    // Entorno: dónde está la cámara, si está bajo el agua o en una cueva (la oscuridad entra y sale poco a poco).
    const XMFLOAT3& player = scene.player.body.position;
    Environment env;
    env.eye = scene.camera.eye(player);
    env.focus = player;
    env.tide = scene.world.tide;
    env.underwater = scene.player.diving() && env.eye.y < scene.world.waterLevel(env.eye.x, env.eye.z, scene.player.body.feet());
    env.worldRadius = scene.habitats.radius();
    m_darkness += (scene.habitats.darkness(player.x, player.z, scene.player.body.feet()) - m_darkness) * 0.08f;
    env.darkness = m_darkness;
    m_eye = env.eye;

    const DayCycle::Lighting light = scene.dayCycle.lighting();
    const XMMATRIX view = scene.getViewMatrix();
    const XMMATRIX viewProj = view * XMLoadFloat4x4(&m_proj);
    const XMMATRIX lightMatrix = lightViewProj(light.lightDir, env.focus);
    CrashUtil::stage("dibujo: recoger objetos");
    collect(scene);
    CrashUtil::stage("dibujo: constantes");
    updateFrame(context, light, view, lightMatrix, env);

    ID3D11Buffer* buffers[2] = { m_objectBuffer.Get(), m_frameBuffer.Get() };
    context->VSSetConstantBuffers(0, 2, buffers);
    context->PSSetConstantBuffers(0, 2, buffers);

    CrashUtil::stage("dibujo: sombras");
    // 1. Mapa de sombras: la luz activa (sol o luna) ve todos los objetos cercanos.
    renderShadowMap(context, lightMatrix);

    // 2. Cielo (cubre toda la pantalla; no usa la profundidad).
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_skyVertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_skyPixelShader.Get(), nullptr, 0);
    context->OMSetDepthStencilState(m_skyDepth.Get(), 0);
    context->Draw(3, 0);
    context->OMSetDepthStencilState(nullptr, 0);

    // 3. Relieve y objetos, iluminados por el sol o la luna y con las sombras del mapa.
    ID3D11ShaderResourceView* shadowSrv = m_shadowSrv.Get();
    ID3D11SamplerState* shadowSampler = m_shadowSampler.Get();
    context->PSSetShaderResources(0, 1, &shadowSrv);
    ID3D11SamplerState* modelSampler = m_modelSampler.Get();
    context->PSSetSamplers(0, 1, &shadowSampler);
    context->PSSetSamplers(1, 1, &modelSampler);
    context->IASetInputLayout(m_inputLayout.Get());
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    CrashUtil::stage("dibujo: relieve");
    drawChunks(context, m_terrain, viewProj, kTerrainDrawRange);
    CrashUtil::stage("dibujo: galerías");
    drawChunks(context, m_cave, viewProj, kCaveDrawRange);
    drawChunks(context, m_skyBase, viewProj, kTerrainDrawRange);
    CrashUtil::stage("dibujo: objetos");
    drawAll(context, viewProj, false);

    CrashUtil::stage("dibujo: agua");
    // 4. El agua, translúcida, encima de todo lo anterior.
    drawWater(context, viewProj);

    CrashUtil::stage("dibujo: brillos");
    // 5. Brillos aditivos (rayo de captura, destellos).
    drawGlow(context, viewProj);
}

void Renderer3D::cleanup() {
    m_draws.clear();
    m_models.clear();
    m_ballModels.clear();
    m_beam = {};
    m_modelSampler.Reset();
    m_additiveBlend.Reset();
    m_glowPixelShader.Reset();
    m_modelPixelShader.Reset();
    m_noCull.Reset();
    m_waterDepth.Reset();
    m_waterBlend.Reset();
    m_waterPixelShader.Reset();
    m_waterVertexShader.Reset();
    m_shadowRaster.Reset();
    m_shadowSampler.Reset();
    m_shadowSrv.Reset();
    m_shadowDsv.Reset();
    m_skyDepth.Reset();
    m_frameBuffer.Reset();
    m_objectBuffer.Reset();
    m_skyPixelShader.Reset();
    m_skyVertexShader.Reset();
    m_shadowVertexShader.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_sphere = {};
    m_dome = {};
    m_star = {};
    m_cube = {};
    m_cone = {};
    m_ocean = {};
    m_water.clear();
    m_terrain.clear();
    m_cave.clear();
    m_caveWater.clear();
    m_worldBuilt = false;
}
