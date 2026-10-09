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

    // Mapa de sombras: una proyección ortográfica fija que cubre todo el mundo (así las sombras no "nadan"
    // al moverse el jugador) vista desde la dirección de la luz activa.
    constexpr float kNodeDrawRange = 90.0f; // los materiales de recolección más lejos no se dibujan
    constexpr UINT kShadowMapSize = 3072;
    constexpr float kShadowHalfExtent = 92.0f; // cubre la diagonal del mundo (128 x 128)
    constexpr float kLightDistance = 140.0f;
    constexpr float kLightNear = 20.0f;
    constexpr float kLightFar = 260.0f;
    constexpr float kShadowNormalOffset = 0.05f; // evita el moteado en superficies casi paralelas a la luz
    constexpr float kShadowDepthBias = 0.0004f;

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

        float4 PS(PS_IN input) : SV_Target {
            float3 n = normalize(input.nrm);
            float diffuse = saturate(dot(n, LightDir.xyz));
            float lit = 0.0f;
            if (diffuse > 0.0f) lit = Shadow(input.wpos, n);
            float3 ambient = lerp(AmbientGround.rgb, AmbientSky.rgb, n.y * 0.5f + 0.5f);
            // El alfa del vértice indica cuánto se tiñe (1 = todo, 0 = conserva su color); el alfa del tinte es el brillo propio.
            float3 albedo = lerp(input.col.rgb, input.col.rgb * Tint.rgb, input.col.a);
            float3 shaded = albedo * (ambient + LightColor.rgb * diffuse * lit);
            return float4(saturate(lerp(shaded, albedo, Tint.a)), 1.0f);
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

            return float4(saturate(sky), 1.0f);
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

// Suelo en tablero: 4 vértices y 6 índices por baldosa, generado una sola vez.
Renderer3D::Mesh Renderer3D::createFloor(ID3D11Device* device, const HabitatMap& habitats, const GameData& data) {
    constexpr float tile = 2.0f;
    constexpr int tiles = static_cast<int>(2.0f * Scene::HALF_SIZE / tile);
    static_assert(tiles * tiles * 4 <= 65535, "El suelo no cabe en índices de 16 bits");

    const XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
    constexpr float CHECKER_SHADE = 0.88f; // las baldosas alternas son algo más oscuras
    constexpr XMFLOAT3 DEFAULT_GROUND = { 0.22f, 0.62f, 0.23f };

    // Color del suelo en una esquina: mezcla de los colores de los hábitats según su peso allí.
    std::vector<XMFLOAT3> habitatColors;
    for (const Habitat& habitat : data.habitats) habitatColors.push_back(HabitatStyle::color(habitat.name));
    std::vector<float> weights;
    const auto groundAt = [&](float x, float z, float shade) {
        XMFLOAT3 color = habitatColors.empty() ? DEFAULT_GROUND : XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        habitats.weights(x, z, weights);
        for (size_t i = 0; i < weights.size() && i < habitatColors.size(); ++i) {
            color.x += weights[i] * habitatColors[i].x;
            color.y += weights[i] * habitatColors[i].y;
            color.z += weights[i] * habitatColors[i].z;
        }
        return XMFLOAT4{ color.x * shade, color.y * shade, color.z * shade, 1.0f };
    };

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(static_cast<size_t>(tiles) * tiles * 4);
    indices.reserve(static_cast<size_t>(tiles) * tiles * 6);

    for (int row = 0; row < tiles; ++row) {
        for (int col = 0; col < tiles; ++col) {
            const float x0 = -Scene::HALF_SIZE + col * tile;
            const float z0 = -Scene::HALF_SIZE + row * tile;
            const float x1 = x0 + tile;
            const float z1 = z0 + tile;
            const float shade = ((row + col) & 1) ? CHECKER_SHADE : 1.0f;

            const int first = static_cast<int>(vertices.size());
            vertices.push_back({ { x0, 0.0f, z0 }, up, groundAt(x0, z0, shade) });
            vertices.push_back({ { x0, 0.0f, z1 }, up, groundAt(x0, z1, shade) });
            vertices.push_back({ { x1, 0.0f, z0 }, up, groundAt(x1, z0, shade) });
            vertices.push_back({ { x1, 0.0f, z1 }, up, groundAt(x1, z1, shade) });
            for (const int i : { 0, 1, 2, 2, 1, 3 }) indices.push_back(static_cast<uint16_t>(first + i));
        }
    }
    return createMesh(device, vertices.data(), static_cast<UINT>(vertices.size()), sizeof(Vertex), indices);
}

// Jugador: cápsula (cilindro con un cono arriba y otro abajo), con color degradado de rojo claro a oscuro.
Renderer3D::Mesh Renderer3D::createPlayer(ID3D11Device* device) {
    constexpr int segments = 12;
    constexpr float radius = 0.4f;
    const XMFLOAT4 light = { 0.9f, 0.2f, 0.2f, 1.0f };
    const XMFLOAT4 dark = { 0.7f, 0.1f, 0.1f, 1.0f };

    // Orden de vértices: anillo inferior [0, segments), anillo superior [segments, 2*segments), punta inferior y punta superior.
    std::vector<Vertex> vertices;
    vertices.reserve(2 * segments + 2);
    for (int i = 0; i < segments; ++i) {
        const float theta = static_cast<float>(i) / segments * XM_2PI;
        vertices.push_back({ { std::cos(theta) * radius, 0.2f, std::sin(theta) * radius }, ringNormal(theta, -0.35f), light });
    }
    for (int i = 0; i < segments; ++i) {
        const float theta = static_cast<float>(i) / segments * XM_2PI;
        vertices.push_back({ { std::cos(theta) * radius, 1.2f, std::sin(theta) * radius }, ringNormal(theta, 0.35f), dark });
    }
    vertices.push_back({ { 0.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, light });
    vertices.push_back({ { 0.0f, 1.4f, 0.0f }, { 0.0f, 1.0f, 0.0f }, dark });

    const int tipBottom = 2 * segments;
    const int tipTop = tipBottom + 1;

    // Sentido horario visto desde fuera (cara frontal en D3D11 por defecto).
    std::vector<uint16_t> indices;
    indices.reserve(segments * 12);
    for (int i = 0; i < segments; ++i) {
        const int j = (i + 1) % segments;
        const int bottomI = i, bottomJ = j, topI = segments + i, topJ = segments + j;
        for (const int index : { bottomI, topI, bottomJ,      // cuerpo
                                 bottomJ, topI, topJ,
                                 bottomI, bottomJ, tipBottom, // cono inferior
                                 topJ, topI, tipTop }) {      // cono superior
            indices.push_back(static_cast<uint16_t>(index));
        }
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
    m_player = createPlayer(device);
    m_cube = createCube(device);
    m_sphere = createSphere(device);
    m_dome = createSphere(device, true);
    m_star = createStar(device);
    m_draws.reserve(32);
}

XMMATRIX Renderer3D::lightViewProj(const XMFLOAT3& lightDir) {
    const XMVECTOR toLight = XMVector3Normalize(XMLoadFloat3(&lightDir));
    const XMMATRIX view = XMMatrixLookAtLH(XMVectorScale(toLight, kLightDistance), XMVectorZero(), g_XMIdentityR1);
    return view * XMMatrixOrthographicLH(2.0f * kShadowHalfExtent, 2.0f * kShadowHalfExtent, kLightNear, kLightFar);
}

void Renderer3D::add(const Mesh& mesh, CXMMATRIX world, const XMFLOAT4& tint, bool castsShadow) {
    Draw draw = { &mesh, {}, tint, castsShadow };
    XMStoreFloat4x4(&draw.world, world);
    m_draws.push_back(draw);
}

// Lista única de lo que hay en el mundo: la usan el mapa de sombras y la pantalla.
void Renderer3D::collect(const Scene& scene) {
    m_draws.clear();

    for (const Physics::World::Box& wall : scene.world.obstacles) {
        add(m_cube, wallWorld(wall), kWallTint);
    }

    const XMFLOAT3& p = scene.player.body.position;
    add(m_player, XMMatrixScaling(1.0f, scene.player.heightScale(), 1.0f) * XMMatrixTranslation(p.x, p.y, p.z), kWhite);

    for (const CaptureTarget* wild : scene.wild.loaded()) {
        const CaptureTarget& target = *wild;
        const float size = target.scale();
        if (size > 0.001f) addCreature(target.drawCenter(), size, target.yaw(), target.speciesId(), target.shiny());

        // Animación de captura: la bola (tambaleándose) y las estrellas, que brillan y no proyectan sombra.
        if (const CaptureSequence* sequence = target.sequence()) {
            addBall(sequence->ball(), sequence->ballTilt(), sequence->ballScale(), sequence->ballGlow());

            const XMFLOAT3 color = sequence->starColor();
            for (int i = 0; i < sequence->starCount(); ++i) {
                XMFLOAT3 position;
                float scale, spin;
                sequence->star(i, position, scale, spin);
                if (scale > 0.001f) {
                    add(m_star, XMMatrixScaling(scale, scale, scale) * XMMatrixRotationY(spin) * XMMatrixTranslation(position.x, position.y, position.z),
                        { color.x, color.y, color.z, 1.0f }, false);
                }
            }
        }
    }

    if (scene.companion.active()) {
        const Companion& pet = scene.companion;
        const float size = Companion::SIZE * pet.scale();
        if (size > 0.001f) {
            addCreature({ pet.body.position.x, pet.body.position.y + size * 0.5f + pet.bob(), pet.body.position.z },
                        size, pet.yaw(), pet.speciesId(), scene.storage.active() && scene.storage.active()->shiny);
        }
        if (const Pokeball* ball = pet.ball()) addBall(*ball, 0.0f, pet.ballScale(), 0.3f); // cambio de pokémon
    }

    for (const Chest& chest : scene.chests.entities) addChest(chest);
    for (const ResourceNode& node : scene.nodes) {
        const float dx = node.body.position.x - p.x, dz = node.body.position.z - p.z;
        if (dx * dx + dz * dz <= kNodeDrawRange * kNodeDrawRange) addNode(node);
    }
    for (const GroundItem& item : scene.groundItems.entities) addGroundItem(item);
    addMachine();

    for (const Pokeball& ball : scene.balls) addBall(ball, 0.0f, 1.0f, 0.0f);
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
        constexpr float STUMP_UNTIL = 0.2f;  // hasta este avance solo se ve el tocón
        constexpr float STUMP_HEIGHT = 0.4f;
        if (g < STUMP_UNTIL) {
            part({ 0.55f, STUMP_HEIGHT, 0.55f }, 0.0f, { 0.0f, STUMP_HEIGHT * 0.5f, 0.0f }, look.body, 0.0f);
            part({ 0.4f, 0.03f, 0.4f }, 0.0f, { 0.0f, STUMP_HEIGHT + 0.01f, 0.0f }, { look.body.x * 1.35f, look.body.y * 1.3f, look.body.z * 1.2f }, 0.0f);
            return;
        }
        const float k = 0.25f + 0.75f * (g - STUMP_UNTIL) / (1.0f - STUMP_UNTIL); // el árbol joven crece entero desde el tocón
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
    const float body = look.specks ? 1.0f : 0.3f + 0.7f * g;
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

void Renderer3D::addBall(const Pokeball& ball, float tilt, float scale, float glow) {
    if (scale <= 0.001f) return;

    const XMFLOAT3& b = ball.body.position;
    const float r = Pokeball::RADIUS * scale;
    add(m_sphere, XMMatrixScaling(r, r, r) * XMMatrixRotationZ(tilt) * XMMatrixTranslation(b.x, b.y, b.z),
        { ball.color.x, ball.color.y, ball.color.z, glow });
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
    context->IASetIndexBuffer(mesh.indices.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(mesh.indexCount, 0, 0);
}

// Dibuja todos los objetos del mundo con la cámara dada (la de la luz o la del jugador).
void Renderer3D::drawAll(ID3D11DeviceContext* context, CXMMATRIX viewProj, bool shadowPass) const {
    for (const Draw& draw : m_draws) {
        if (shadowPass && !draw.castsShadow) continue;
        const XMMATRIX world = XMLoadFloat4x4(&draw.world);
        setObject(context, world, world * viewProj, draw.tint);
        drawIndexed(context, *draw.mesh);
    }
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

void Renderer3D::updateFrame(ID3D11DeviceContext* context, const DayCycle::Lighting& light, CXMMATRIX view, CXMMATRIX lightViewProj) {
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
    context->UpdateSubresource(m_frameBuffer.Get(), 0, nullptr, &fc, 0, 0);
}

void Renderer3D::render(ID3D11DeviceContext* context, const Scene& scene, int width, int height) {
    if (width <= 0 || height <= 0) return;

    if (!m_floorBuilt || m_floorSeed != scene.habitats.seed()) {
        m_floor = createFloor(m_device, scene.habitats, scene.data());
        m_floorSeed = scene.habitats.seed();
        m_floorBuilt = true;
    }

    const float aspect = static_cast<float>(width) / height;
    const float fov = scene.camera.fov();
    if (aspect != m_aspect || fov != m_fov) {
        m_aspect = aspect;
        m_fov = fov;
        XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(fov, aspect, 0.1f, 250.0f));
    }

    const DayCycle::Lighting light = scene.dayCycle.lighting();
    const XMMATRIX view = scene.getViewMatrix();
    const XMMATRIX viewProj = view * XMLoadFloat4x4(&m_proj);
    const XMMATRIX lightMatrix = lightViewProj(light.lightDir);
    collect(scene);
    updateFrame(context, light, view, lightMatrix);

    ID3D11Buffer* buffers[2] = { m_objectBuffer.Get(), m_frameBuffer.Get() };
    context->VSSetConstantBuffers(0, 2, buffers);
    context->PSSetConstantBuffers(0, 2, buffers);

    // 1. Mapa de sombras: la luz activa (sol o luna) ve todos los objetos.
    renderShadowMap(context, lightMatrix);

    // 2. Cielo (cubre toda la pantalla; no usa la profundidad).
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_skyVertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_skyPixelShader.Get(), nullptr, 0);
    context->OMSetDepthStencilState(m_skyDepth.Get(), 0);
    context->Draw(3, 0);
    context->OMSetDepthStencilState(nullptr, 0);

    // 3. Suelo y objetos, iluminados por el sol o la luna y con las sombras del mapa.
    ID3D11ShaderResourceView* shadowSrv = m_shadowSrv.Get();
    ID3D11SamplerState* shadowSampler = m_shadowSampler.Get();
    context->PSSetShaderResources(0, 1, &shadowSrv);
    context->PSSetSamplers(0, 1, &shadowSampler);
    context->IASetInputLayout(m_inputLayout.Get());
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    setObject(context, XMMatrixIdentity(), viewProj, kWhite);
    drawIndexed(context, m_floor);
    drawAll(context, viewProj, false);
}

void Renderer3D::cleanup() {
    m_draws.clear();
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
    m_player = {};
    m_floor = {};
    m_floorBuilt = false;
}
