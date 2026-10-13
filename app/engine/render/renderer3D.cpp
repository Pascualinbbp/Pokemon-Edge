#include "renderer3D.hpp"
#include "../../utils/graphics/d3dUtil.hpp"
#include "../world/style/habitatStyle.hpp"
#include "../world/style/itemStyle.hpp"
#include "../world/style/pokemonStyle.hpp"
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
            color *= lerp(1.0f, torch * torch + 0.03f, Env.x);
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
        // (0 mar, 1 río, 2 lago, 3 charca de pantano, 4 cascada, 5 mar abierto hasta el horizonte, 6 agua de nube).
        struct WATER_IN { float4 pos : SV_POSITION; float3 wpos : TEXCOORD0; float3 info : TEXCOORD1; float kind : TEXCOORD2; };

        float WaveHeight(float2 p, float kind, float t) {
            float amp = kind < 0.5f ? 0.22f : kind < 1.5f ? 0.05f : kind < 2.5f ? 0.09f : kind < 3.5f ? 0.02f : 0.10f;
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
            bool open = k > 4.5f && k < 5.5f, cloud = k > 5.5f;
            if (open) clip(length(pxz) - (Env.w - 8.0f)); // el mar abierto solo fuera del archipiélago
            float t = SkyParams.w;
            float kk = cloud ? 3.0f : open ? 0.0f : k;

            float e = 0.35f;
            float h0 = WaveHeight(pxz, kk, t);
            float hx = WaveHeight(pxz + float2(e, 0.0f), kk, t);
            float hz = WaveHeight(pxz + float2(0.0f, e), kk, t);
            float3 n = normalize(float3(-(hx - h0) / e * 2.5f, 1.0f, -(hz - h0) / e * 2.5f));
            float3 v = normalize(CameraPos.xyz - input.wpos);
            float depth = input.info.x;
            float2 flow = input.info.yz;

            float3 shallow, deep;
            float full;
            if (cloud)                { shallow = float3(0.93f, 0.96f, 1.0f); deep = float3(0.60f, 0.73f, 0.95f); full = 2.0f; }
            else if (k < 0.5f || open) { shallow = float3(0.30f, 0.80f, 0.84f); deep = float3(0.03f, 0.18f, 0.42f); full = 7.0f; }
            else if (k < 1.5f)        { shallow = float3(0.38f, 0.74f, 0.74f); deep = float3(0.10f, 0.36f, 0.52f); full = 2.5f; }
            else if (k < 2.5f)        { shallow = float3(0.30f, 0.68f, 0.74f); deep = float3(0.06f, 0.28f, 0.46f); full = 4.0f; }
            else if (k < 3.5f)        { shallow = float3(0.30f, 0.46f, 0.16f); deep = float3(0.10f, 0.24f, 0.10f); full = 0.8f; }
            else                      { shallow = float3(0.80f, 0.92f, 0.96f); deep = float3(0.45f, 0.70f, 0.85f); full = 1.5f; }
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
                const float rock = smooth(0.55f, 1.0f, slope);                          // lo empinado es roca
                color = mix(color, ROCK, rock);
                const float line = HabitatMap::SNOW_LINE + 2.5f * std::sin(x * 0.11f) * std::cos(z * 0.09f); // línea de nieve irregular
                const float snow = smooth(line - 2.0f, line + 2.5f, height) * (1.0f - 0.65f * rock);
                color = mix(color, SNOW, snow);
                color = mix(color, ICE, smooth(line + 10.0f, line + 22.0f, height) * (0.35f + 0.65f * rock)); // y hielo más arriba
            }
        }
        const float tone = 0.96f + 0.04f * std::sin(x * 1.7f) * std::cos(z * 1.3f); // variación fina para que el relieve se lea
        return Vertex{ { x, height, z }, { -gx / length, 1.0f / length, -gz / length }, { color.x * tone, color.y * tone, color.z * tone, 1.0f } };
    };
    const auto keepCell = [&](int col, int row) {
        const float x = terrain.nodeCoord(col) + Physics::Heightfield::CELL * 0.5f, z = terrain.nodeCoord(row) + Physics::Heightfield::CELL * 0.5f;
        if (!habitats.playable(x, z)) return false;
        const float void_ = HabitatMap::SKY_VOID * 0.5f;
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
            default:                                              return 0.0f;
        }
    };

    const auto vertexAt = [&](int col, int row) {
        float y = level.at(col, row), depth = (std::max)(0.0f, level.at(col, row) - ground.at(col, row)), kind = 0.0f;
        if (wet(col, row)) kind = code(habitats.kindAtNode(col, row));
        else { // el borde de la orilla toma el nivel y el tipo del agua que tiene al lado
            float sum = 0.0f;
            int count = 0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (!wet(col + dc, row + dr)) continue;
                    sum += level.at(col + dc, row + dr);
                    kind = code(habitats.kindAtNode(col + dc, row + dr));
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
    m_ballModel = m_models.get(device, "pokeball", true); // si falta el archivo se dibuja la esfera sencilla
    m_ballLid = m_ballModel ? m_ballModel->part("movable") : -1;
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
void Renderer3D::collect(const Scene& scene) {
    m_draws.clear();

    for (const Physics::World::Box& wall : scene.world.obstacles) {
        add(m_cube, wallWorld(wall), kWallTint);
    }

    const XMFLOAT3& p = scene.player.body.position;
    addPlayer(scene);
    const float time = scene.dayCycle.lighting().time;
    addSky(scene, time);

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
            addCreature({ pet.body.position.x, pet.body.position.y + size * 0.5f + pet.bob(), pet.body.position.z },
                        size, pet.yaw(), pet.speciesId(), scene.storage.active() && scene.storage.active()->shiny);
        }
        if (const Pokeball* ball = pet.ball()) addBall(*ball, pet.ballScale()); // cambio de pokémon
    }

    for (const Chest& chest : scene.chests.entities) addChest(chest);
    for (const ResourceNode& node : scene.nodes) {
        const float dx = node.body.position.x - p.x, dz = node.body.position.z - p.z;
        if (dx * dx + dz * dz <= kNodeDrawRange * kNodeDrawRange) addNode(node);
    }
    for (const GroundItem& item : scene.groundItems.entities) addGroundItem(item);
    addMachine();

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

// Isla del cielo: la base rocosa, las nubes alrededor y, en el mar, la corriente ascendente que lleva hasta ella.
void Renderer3D::addSky(const Scene& scene, float time) {
    const XMFLOAT3& p = scene.player.body.position;
    for (const HabitatMap::Region& region : scene.habitats.regions()) {
        if (region.kind != HabitatPlacement::SKY) continue;
        const float dx = region.x - p.x, dz = region.z - p.z;
        if (dx * dx + dz * dz > 520.0f * 520.0f) continue;
        const float floorY = region.floorY;
        const float radius = region.radius - 2.0f;
        add(m_cone, XMMatrixScaling(radius, 62.0f, radius) * XMMatrixTranslation(region.x, floorY - 0.5f, region.z), kWhite, false);

        // Nubes: grupos de bloques blancos aplastados, repartidos de forma fija alrededor y debajo de la isla.
        for (int i = 0; i < 26; ++i) {
            const float h1 = std::fmod(std::sin(i * 12.9898f) * 43758.5453f, 1.0f), h2 = std::fmod(std::sin(i * 78.233f) * 12345.6789f, 1.0f), h3 = std::fmod(std::sin(i * 39.346f) * 9876.543f, 1.0f);
            const float angle = std::fabs(h1) * XM_2PI, dist = region.radius + 10.0f + std::fabs(h2) * 170.0f;
            const float cx = region.x + std::cos(angle) * dist + std::sin(time * 0.05f + i) * 3.0f, cz = region.z + std::sin(angle) * dist;
            const float cy = floorY - 25.0f - std::fabs(h3) * 70.0f;
            for (int k = 0; k < 4; ++k) {
                const float s = 7.0f + 5.0f * std::fabs(std::sin(i * 3.1f + k * 1.7f));
                add(m_cube, XMMatrixScaling(s * 1.6f, s * 0.45f, s) * XMMatrixTranslation(cx + (k - 1.5f) * s * 0.9f, cy + (k & 1) * 1.5f, cz + std::sin(k * 2.3f + i) * s * 0.5f), { 1.0f, 1.0f, 1.0f, 0.65f }, false);
            }
        }
    }
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

// Pokémon (salvaje o compañero): cubo del color de su especie con un morro amarillo que marca hacia dónde mira.
void Renderer3D::addCreature(const XMFLOAT3& c, float size, float yaw, int speciesId, bool shiny) {
    const XMFLOAT3 base = PokemonStyle::color(speciesId);
    const XMFLOAT3 color = shiny ? PokemonStyle::shiny(base) : base;
    add(m_cube, XMMatrixScaling(size, size, size) * XMMatrixTranslation(c.x, c.y, c.z), { color.x, color.y, color.z, shiny ? 0.3f : 0.0f });

    const float nose = size * 0.4f;
    add(m_cube, XMMatrixScaling(nose, nose, nose) *
        XMMatrixTranslation(c.x + std::sin(yaw) * size * 0.5f, c.y + size * 0.15f, c.z + std::cos(yaw) * size * 0.5f), kNoseTint);
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
    if (look.shape == Shape::TREE || look.shape == Shape::CONIFER || look.shape == Shape::PALM) {
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

// Pokéball: el modelo .glb (o una esfera si no se pudo cargar), centrado en 'position', del tamaño de la física, orientado según
// yaw (hacia dónde mira el botón), spin (rodando) y roll (balanceo sobre el eje del botón), con la tapa abierta 'lid' (0..1).
void Renderer3D::addBall(const XMFLOAT3& position, float yaw, float spin, float roll, float lid, float scale, const XMFLOAT3& color, float glow) {
    if (scale <= 0.001f) return;
    const XMMATRIX orient = XMMatrixRotationZ(roll) * XMMatrixRotationX(spin) * XMMatrixRotationY(yaw) * XMMatrixTranslation(position.x, position.y, position.z);
    if (!m_ballModel) {
        const float r = Pokeball::RADIUS * scale;
        add(m_sphere, XMMatrixScaling(r, r, r) * orient, { color.x, color.y, color.z, glow });
        return;
    }
    const XMFLOAT3 c = m_ballModel->center();
    const float s = Pokeball::RADIUS * 2.0f * scale / m_ballModel->height();
    ModelPose pose;
    pose.set(m_ballLid, lid);
    addModel(*m_ballModel, XMMatrixTranslation(-c.x, -c.y, -c.z) * XMMatrixScaling(s, s, s) * orient, { color.x, color.y, color.z, glow }, pose);
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
    addBall(pose.position, pose.yaw, pose.spin, pose.roll, pose.lid, pose.scale, sequence.ball().color, pose.glow);

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
void Renderer3D::drawChunks(ID3D11DeviceContext* context, const std::vector<Chunk>& chunks, CXMMATRIX viewProj) const {
    setObject(context, XMMatrixIdentity(), viewProj, kWhite);
    for (const Chunk& chunk : chunks) {
        const float dx = chunk.center.x - m_eye.x, dy = chunk.center.y - m_eye.y, dz = chunk.center.z - m_eye.z;
        const float reach = kTerrainDrawRange + chunk.radius;
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

    drawChunks(context, m_water, viewProj);
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
    context->UpdateSubresource(m_frameBuffer.Get(), 0, nullptr, &fc, 0, 0);
}

void Renderer3D::render(ID3D11DeviceContext* context, const Scene& scene, int width, int height) {
    if (width <= 0 || height <= 0) return;

    if (!m_worldBuilt || m_worldSeed != scene.habitats.seed()) {
        m_terrain = createTerrain(m_device, scene.habitats, scene.world, scene.data());
        m_water = createWater(m_device, scene.habitats, scene.world);
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
    env.underwater = scene.player.diving() && env.eye.y < scene.world.waterLevel(env.eye.x, env.eye.z);
    env.worldRadius = scene.habitats.radius();
    m_darkness += (scene.habitats.darkness(player.x, player.z) - m_darkness) * 0.08f;
    env.darkness = m_darkness;
    m_eye = env.eye;

    const DayCycle::Lighting light = scene.dayCycle.lighting();
    const XMMATRIX view = scene.getViewMatrix();
    const XMMATRIX viewProj = view * XMLoadFloat4x4(&m_proj);
    const XMMATRIX lightMatrix = lightViewProj(light.lightDir, env.focus);
    collect(scene);
    updateFrame(context, light, view, lightMatrix, env);

    ID3D11Buffer* buffers[2] = { m_objectBuffer.Get(), m_frameBuffer.Get() };
    context->VSSetConstantBuffers(0, 2, buffers);
    context->PSSetConstantBuffers(0, 2, buffers);

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

    drawChunks(context, m_terrain, viewProj);
    drawAll(context, viewProj, false);

    // 4. El agua, translúcida, encima de todo lo anterior.
    drawWater(context, viewProj);

    // 5. Brillos aditivos (rayo de captura, destellos).
    drawGlow(context, viewProj);
}

void Renderer3D::cleanup() {
    m_draws.clear();
    m_models.clear();
    m_ballModel = nullptr;
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
    m_worldBuilt = false;
}
