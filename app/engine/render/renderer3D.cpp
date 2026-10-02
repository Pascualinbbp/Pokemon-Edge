#include "renderer3D.hpp"
#include "../../utils/graphics/d3dUtil.hpp"
#include "../../utils/core/loggerUtil.hpp"
#include <d3dcompiler.h>
#include <cstring>
#include <vector>

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

Renderer3D::Mesh Renderer3D::createMesh(ID3D11Device* device, const Vertex* vertices, UINT count) {
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * count);
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    const D3D11_SUBRESOURCE_DATA data = { vertices, 0, 0 };
    Mesh mesh;
    mesh.vertexCount = count;
    D3dUtil::check(device->CreateBuffer(&desc, &data, &mesh.buffer), "CreateBuffer (vertex)");
    return mesh;
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
    const XMFLOAT4 green = { 0.2f, 0.8f, 0.2f, 1.0f };
    const Vertex floorVerts[] = {
        { { -10.0f, 0.0f, -10.0f }, green }, { { -10.0f, 0.0f, 10.0f }, green }, { { 10.0f, 0.0f, -10.0f }, green },
        { {  10.0f, 0.0f, -10.0f }, green }, { { -10.0f, 0.0f, 10.0f }, green }, { { 10.0f, 0.0f,  10.0f }, green },
    };
    m_floor = createMesh(device, floorVerts, _countof(floorVerts));

    // Jugador: Píldora / Cápsula 3D básica (aproximación con cilindro y cúpulas superior e inferior)
    const XMFLOAT4 capColor = { 0.9f, 0.2f, 0.2f, 1.0f }; // Color rojo/rosita típico de cápsula
    const XMFLOAT4 capColorDark = { 0.7f, 0.1f, 0.1f, 1.0f };
    
    std::vector<Vertex> capsuleVerts;
    float radius = 0.4f;
    float height = 1.2f;
    int segments = 8;

    // Generar cuerpo cilíndrico de la píldora
    for (int i = 0; i < segments; ++i) {
        float theta1 = (float)i / segments * 2.0f * DirectX::XM_PI;
        float theta2 = (float)(i + 1) / segments * 2.0f * DirectX::XM_PI;

        float x1 = cosf(theta1) * radius;
        float z1 = sinf(theta1) * radius;
        float x2 = cosf(theta2) * radius;
        float z2 = sinf(theta2) * radius;

        // Dos triángulos por segmento de cuerpo
        capsuleVerts.push_back({ { x1, 0.2f, z1 }, capColor });
        capsuleVerts.push_back({ { x2, 0.2f, z2 }, capColor });
        capsuleVerts.push_back({ { x1, 1.2f, z1 }, capColorDark });

        capsuleVerts.push_back({ { x2, 0.2f, z2 }, capColor });
        capsuleVerts.push_back({ { x2, 1.2f, z2 }, capColorDark });
        capsuleVerts.push_back({ { x1, 1.2f, z1 }, capColorDark });

        // Tapa inferior (cono hacia y = 0.0f)
        capsuleVerts.push_back({ { x1, 0.2f, z1 }, capColor });
        capsuleVerts.push_back({ { x2, 0.2f, z2 }, capColor });
        capsuleVerts.push_back({ { 0.0f, 0.0f, 0.0f }, capColor });

        // Tapa superior (cono hacia y = 1.4f)
        capsuleVerts.push_back({ { x2, 1.2f, z2 }, capColorDark });
        capsuleVerts.push_back({ { x1, 1.2f, z1 }, capColorDark });
        capsuleVerts.push_back({ { 0.0f, 1.4f, 0.0f }, capColorDark });
    }

    m_player = createMesh(device, capsuleVerts.data(), static_cast<UINT>(capsuleVerts.size()));
}

void Renderer3D::drawMesh(ID3D11DeviceContext* context, const Mesh& mesh, CXMMATRIX world, CXMMATRIX viewProj) const {
    ConstantBuffer cb;
    XMStoreFloat4x4(&cb.worldViewProj, XMMatrixTranspose(world * viewProj));
    context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* vb = mesh.buffer.Get();
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context->Draw(mesh.vertexCount, 0);
}

void Renderer3D::render(ID3D11DeviceContext* context, const Scene& scene, int width, int height) {
    if (width <= 0 || height <= 0) return;

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    ID3D11Buffer* cb = m_constantBuffer.Get();
    context->VSSetConstantBuffers(0, 1, &cb);

    const XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, static_cast<float>(width) / height, 0.1f, 100.0f);
    const XMMATRIX viewProj = scene.getViewMatrix() * proj;

    const XMFLOAT3& p = scene.player.position;
    drawMesh(context, m_floor, XMMatrixIdentity(), viewProj);
    drawMesh(context, m_player, XMMatrixTranslation(p.x, p.y, p.z), viewProj);
}

void Renderer3D::cleanup() {
    m_player = {};
    m_floor = {};
    m_constantBuffer.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
}