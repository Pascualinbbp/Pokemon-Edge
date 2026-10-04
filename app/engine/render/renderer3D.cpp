#include "renderer3D.hpp"
#include "../../utils/graphics/d3dUtil.hpp"
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

    const XMFLOAT4 kWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
    const XMFLOAT4 kWallTint = { 0.62f, 0.58f, 0.52f, 1.0f };
    const XMFLOAT4 kTargetTint = { 1.0f, 0.55f, 0.10f, 1.0f };
    const XMFLOAT4 kNoseTint = { 1.0f, 0.90f, 0.15f, 1.0f };

    // Mapa de sombras: una proyección ortográfica fija que cubre todo el mundo (así las sombras no "nadan"
    // al moverse el jugador) vista desde la dirección de la luz activa.
    constexpr UINT kShadowMapSize = 3072;
    constexpr float kShadowHalfExtent = 58.0f; // cubre la diagonal del mundo (80 x 80)
    constexpr float kLightDistance = 90.0f;
    constexpr float kLightNear = 20.0f;
    constexpr float kLightFar = 160.0f;
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
            float3 albedo = input.col.rgb * Tint.rgb;
            return float4(saturate(albedo * (ambient + LightColor.rgb * diffuse * lit)), 1.0f);
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
Renderer3D::Mesh Renderer3D::createFloor(ID3D11Device* device) {
    constexpr float tile = 2.0f;
    constexpr int tiles = static_cast<int>(2.0f * Scene::HALF_SIZE / tile);
    static_assert(tiles * tiles * 4 <= 65535, "El suelo no cabe en índices de 16 bits");

    const XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
    const XMFLOAT4 colors[2] = { { 0.25f, 0.70f, 0.25f, 1.0f }, { 0.18f, 0.55f, 0.20f, 1.0f } };

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
            const XMFLOAT4& color = colors[(row + col) & 1];

            const int first = static_cast<int>(vertices.size());
            vertices.push_back({ { x0, 0.0f, z0 }, up, color });
            vertices.push_back({ { x0, 0.0f, z1 }, up, color });
            vertices.push_back({ { x1, 0.0f, z0 }, up, color });
            vertices.push_back({ { x1, 0.0f, z1 }, up, color });
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

// Esfera unitaria blanca (la normal de cada vértice es su posición); el sombreado lo da la luz.
Renderer3D::Mesh Renderer3D::createSphere(ID3D11Device* device) {
    constexpr int rings = 8;
    constexpr int segments = 12;

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve((rings + 1) * segments);
    indices.reserve(rings * segments * 6);

    for (int r = 0; r <= rings; ++r) {
        const float phi = XM_PI * static_cast<float>(r) / rings; // 0 = polo superior
        const float y = std::cos(phi);
        const float ring = std::sin(phi);
        for (int s = 0; s < segments; ++s) {
            const float theta = static_cast<float>(s) / segments * XM_2PI;
            const XMFLOAT3 p = { std::cos(theta) * ring, y, std::sin(theta) * ring };
            vertices.push_back({ p, p, { 1.0f, 1.0f, 1.0f, 1.0f } });
        }
    }

    const XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
    for (int r = 0; r < rings; ++r) {
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
    m_floor = createFloor(device);
    m_player = createPlayer(device);
    m_cube = createCube(device);
    m_sphere = createSphere(device);
    m_draws.reserve(32);
}

XMMATRIX Renderer3D::lightViewProj(const XMFLOAT3& lightDir) {
    const XMVECTOR toLight = XMVector3Normalize(XMLoadFloat3(&lightDir));
    const XMMATRIX view = XMMatrixLookAtLH(XMVectorScale(toLight, kLightDistance), XMVectorZero(), g_XMIdentityR1);
    return view * XMMatrixOrthographicLH(2.0f * kShadowHalfExtent, 2.0f * kShadowHalfExtent, kLightNear, kLightFar);
}

void Renderer3D::add(const Mesh& mesh, CXMMATRIX world, const XMFLOAT4& tint) {
    Draw draw = { &mesh, {}, tint };
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

    for (const CaptureTarget& target : scene.targets) {
        if (!target.visible()) continue;

        const float size = target.scale();
        const XMFLOAT3 c = target.center();
        add(m_cube, XMMatrixScaling(size, size, size) * XMMatrixTranslation(c.x, c.y, c.z), kTargetTint);

        const float nose = size * 0.4f;
        add(m_cube, XMMatrixScaling(nose, nose, nose) *
            XMMatrixTranslation(c.x + std::sin(target.yaw()) * size * 0.5f, c.y + size * 0.15f, c.z + std::cos(target.yaw()) * size * 0.5f),
            kNoseTint);
    }

    for (const Pokeball& ball : scene.balls) {
        const XMFLOAT3& b = ball.body.position;
        add(m_sphere, XMMatrixScaling(Pokeball::RADIUS, Pokeball::RADIUS, Pokeball::RADIUS) * XMMatrixTranslation(b.x, b.y, b.z),
            { ball.color.x, ball.color.y, ball.color.z, 1.0f });
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
    context->IASetIndexBuffer(mesh.indices.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(mesh.indexCount, 0, 0);
}

// Dibuja todos los objetos del mundo con la cámara dada (la de la luz o la del jugador).
void Renderer3D::drawAll(ID3D11DeviceContext* context, CXMMATRIX viewProj) const {
    for (const Draw& draw : m_draws) {
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
    drawAll(context, lightViewProj);

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
    drawAll(context, viewProj);
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
    m_cube = {};
    m_player = {};
    m_floor = {};
}
