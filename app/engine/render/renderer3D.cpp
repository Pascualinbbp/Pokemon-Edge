#include "renderer3D.hpp"
#include "../../utils/graphics/d3dUtil.hpp"
#include "../../utils/core/loggerUtil.hpp"
#include <d3dcompiler.h>
#include <cmath>
#include <cstring>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace {
    struct ConstantBuffer {
        XMFLOAT4X4 worldViewProj;
    };

    constexpr const char* kShaderCode = R"(
        cbuffer ConstantBuffer : register(b0) { matrix WorldViewProj; }
        struct VS_IN { float3 pos : POSITION; float4 col : COLOR; };
        struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };

        PS_IN VS(VS_IN input) {
            PS_IN output = (PS_IN)0;
            output.pos = mul(float4(input.pos, 1.0f), WorldViewProj);
            output.col = input.col;
            return output;
        }

        float4 PS(PS_IN input) : SV_Target {
            return input.col;
        }
    )";

    ComPtr<ID3DBlob> compileShader(const char* entry, const char* target) {
        ComPtr<ID3DBlob> blob, errors;
        const HRESULT hr = D3DCompile(kShaderCode, std::strlen(kShaderCode), nullptr, nullptr, nullptr,
                                    entry, target, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                    0, &blob, &errors);
        if (FAILED(hr)) {
            if (errors) {
                Logger::logError("RENDERER3D", std::string("Error de shader: ") + static_cast<const char*>(errors->GetBufferPointer()));
            }
            D3dUtil::check(hr, "D3DCompile");
        }
        return blob;
    }
}

Renderer3D::Mesh Renderer3D::createMesh(ID3D11Device* device, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices) {
    Mesh mesh;
    mesh.indexCount = static_cast<UINT>(indices.size());

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data = { vertices.data(), 0, 0 };
    D3dUtil::check(device->CreateBuffer(&desc, &data, &mesh.vertices), "CreateBuffer (vertex)");

    desc.ByteWidth = static_cast<UINT>(sizeof(uint16_t) * indices.size());
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    data.pSysMem = indices.data();
    D3dUtil::check(device->CreateBuffer(&desc, &data, &mesh.indices), "CreateBuffer (index)");
    return mesh;
}

// Suelo en tablero: 4 vértices y 6 índices por baldosa, generado una sola vez.
Renderer3D::Mesh Renderer3D::createFloor(ID3D11Device* device) {
    constexpr float tile = 2.0f;
    constexpr int tiles = static_cast<int>(2.0f * Scene::HALF_SIZE / tile);
    static_assert(tiles * tiles * 4 <= 65535, "El suelo no cabe en índices de 16 bits");

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
            vertices.push_back({ { x0, 0.0f, z0 }, color });
            vertices.push_back({ { x0, 0.0f, z1 }, color });
            vertices.push_back({ { x1, 0.0f, z0 }, color });
            vertices.push_back({ { x1, 0.0f, z1 }, color });
            for (const int i : { 0, 1, 2, 2, 1, 3 }) indices.push_back(static_cast<uint16_t>(first + i));
        }
    }
    return createMesh(device, vertices, indices);
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
        vertices.push_back({ { std::cos(theta) * radius, 0.2f, std::sin(theta) * radius }, light });
    }
    for (int i = 0; i < segments; ++i) {
        const float theta = static_cast<float>(i) / segments * XM_2PI;
        vertices.push_back({ { std::cos(theta) * radius, 1.2f, std::sin(theta) * radius }, dark });
    }
    vertices.push_back({ { 0.0f, 0.0f, 0.0f }, light });
    vertices.push_back({ { 0.0f, 1.4f, 0.0f }, dark });

    const int tipBottom = 2 * segments;
    const int tipTop = tipBottom + 1;

    // Sentido horario visto desde fuera (cara frontal en D3D11 por defecto).
    std::vector<uint16_t> indices;
    indices.reserve(segments * 12);
    for (int i = 0; i < segments; ++i) {
        const int j = (i + 1) % segments;
        const int bottomI = i, bottomJ = j, topI = segments + i, topJ = segments + j;
        for (const int index : { bottomI, topI, bottomJ,    // cuerpo
                                 bottomJ, topI, topJ,
                                 bottomI, bottomJ, tipBottom, // cono inferior
                                 topJ, topI, tipTop }) {      // cono superior
            indices.push_back(static_cast<uint16_t>(index));
        }
    }
    return createMesh(device, vertices, indices);
}

void Renderer3D::init(ID3D11Device* device) {
    // 1. Shaders
    const ComPtr<ID3DBlob> vsBlob = compileShader("VS", "vs_4_0");
    const ComPtr<ID3DBlob> psBlob = compileShader("PS", "ps_4_0");
    D3dUtil::check(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader), "CreateVertexShader");
    D3dUtil::check(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader), "CreatePixelShader");

    // 2. Input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                        D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3dUtil::check(device->CreateInputLayout(layout, _countof(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout), "CreateInputLayout");

    // 3. Constant buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(ConstantBuffer);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3dUtil::check(device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer), "CreateBuffer (constant)");

    // 4. Geometría
    m_floor = createFloor(device);
    m_player = createPlayer(device);
}

void Renderer3D::drawMesh(ID3D11DeviceContext* context, const Mesh& mesh, CXMMATRIX world, CXMMATRIX viewProj) const {
    ConstantBuffer cb;
    XMStoreFloat4x4(&cb.worldViewProj, XMMatrixTranspose(world * viewProj));
    context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* vb = mesh.vertices.Get();
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context->IASetIndexBuffer(mesh.indices.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(mesh.indexCount, 0, 0);
}

void Renderer3D::render(ID3D11DeviceContext* context, const Scene& scene, int width, int height) {
    if (width <= 0 || height <= 0) return;

    const float aspect = static_cast<float>(width) / height;
    if (aspect != m_aspect) {
        m_aspect = aspect;
        XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 250.0f));
    }

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    ID3D11Buffer* cb = m_constantBuffer.Get();
    context->VSSetConstantBuffers(0, 1, &cb);

    const XMMATRIX viewProj = scene.getViewMatrix() * XMLoadFloat4x4(&m_proj);
    const XMFLOAT3& p = scene.player.position;

    drawMesh(context, m_floor, XMMatrixIdentity(), viewProj);
    drawMesh(context, m_player,
        XMMatrixScaling(1.0f, scene.player.heightScale(), 1.0f) * XMMatrixTranslation(p.x, p.y, p.z), viewProj);
}

void Renderer3D::cleanup() {
    m_player = {};
    m_floor = {};
    m_constantBuffer.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
}