#include "renderer3D.hpp"
#include <d3dcompiler.h>

struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
};

// Se debe alinear a 16 bytes para la GPU
__declspec(align(16))
struct ConstantBuffer {
    DirectX::XMMATRIX mWorldViewProj;
};

// Shader básico compilado en tiempo de ejecución (ideal para desarrollo)
const char* shaderCode = R"(
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

void Renderer3D::init(ID3D11Device* device) {
    // 1. Compilar Shaders
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);

    // 2. Input Layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);
    vsBlob->Release(); psBlob->Release();

    // 3. Constant Buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(ConstantBuffer);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);

    // 4. Geometría (Jugador: Cápsula simplificada como un cubo alto/prisma)
    Vertex playerVerts[] = {
        // Base
        { {-0.5f, -1.0f, -0.5f}, {1.0f, 0.2f, 0.2f, 1.0f} }, { {-0.5f, -1.0f, 0.5f}, {1.0f, 0.2f, 0.2f, 1.0f} },
        { { 0.5f, -1.0f,  0.5f}, {1.0f, 0.2f, 0.2f, 1.0f} }, { { 0.5f, -1.0f, -0.5f}, {1.0f, 0.2f, 0.2f, 1.0f} },
        // Top
        { {-0.5f,  1.0f, -0.5f}, {1.0f, 0.5f, 0.5f, 1.0f} }, { {-0.5f,  1.0f, 0.5f}, {1.0f, 0.5f, 0.5f, 1.0f} },
        { { 0.5f,  1.0f,  0.5f}, {1.0f, 0.5f, 0.5f, 1.0f} }, { { 0.5f,  1.0f, -0.5f}, {1.0f, 0.5f, 0.5f, 1.0f} },
    };
    
    // Indices triangulados para el prisma omitidos por brevedad, renderizaremos como Triangles usando 36 vertices reales o LineStrip. 
    // Para asegurar que funciona directo a la primera, hagamos un suelo y un player basico basados en triangulos planos.
    
    Vertex capsuleVerts[36]; 
    // Por simplicidad en este paso, dejaremos el diseño del cubo explícito. 
    // (Omitido código largo de indices, usamos buffers simples listos para usar)

    // Geometría del suelo (Plano verde)
    Vertex floorVerts[] = {
        { {-10.0f, 0.0f, -10.0f}, {0.2f, 0.8f, 0.2f, 1.0f} },
        { {-10.0f, 0.0f,  10.0f}, {0.2f, 0.8f, 0.2f, 1.0f} },
        { { 10.0f, 0.0f, -10.0f}, {0.2f, 0.8f, 0.2f, 1.0f} },

        { { 10.0f, 0.0f, -10.0f}, {0.2f, 0.8f, 0.2f, 1.0f} },
        { {-10.0f, 0.0f,  10.0f}, {0.2f, 0.8f, 0.2f, 1.0f} },
        { { 10.0f, 0.0f,  10.0f}, {0.2f, 0.8f, 0.2f, 1.0f} }
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT; vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    // Creamos suelo
    vbDesc.ByteWidth = sizeof(floorVerts);
    D3D11_SUBRESOURCE_DATA vd = { floorVerts, 0, 0 };
    device->CreateBuffer(&vbDesc, &vd, &m_floorVB);

    // Creamos jugador (Simplificado a un triangulo enorme para no inundar de código de vértices, luego puedes expandirlo)
    Vertex simplePlayer[] = {
        { { 0.0f,  2.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
        { {-0.5f,  0.0f, 0.0f}, {0.8f, 0.0f, 0.0f, 1.0f} },
        { { 0.5f,  0.0f, 0.0f}, {0.8f, 0.0f, 0.0f, 1.0f} }
    };
    vbDesc.ByteWidth = sizeof(simplePlayer);
    vd.pSysMem = simplePlayer;
    device->CreateBuffer(&vbDesc, &vd, &m_playerVB);
}

void Renderer3D::render(ID3D11DeviceContext* context, const Scene& scene, int width, int height) {
    if(width == 0 || height == 0) return;

    // Configurar pipeline
    context->IASetInputLayout(m_inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vertexShader, nullptr, 0);
    context->PSSetShader(m_pixelShader, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &m_constantBuffer);

    // Matrices
    DirectX::XMMATRIX view = scene.getViewMatrix();
    DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, (float)width / (float)height, 0.1f, 100.0f);
    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    // 1. Dibujar Suelo
    ConstantBuffer cb;
    cb.mWorldViewProj = DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity() * view * proj);
    context->UpdateSubresource(m_constantBuffer, 0, nullptr, &cb, 0, 0);
    
    context->IASetVertexBuffers(0, 1, &m_floorVB, &stride, &offset);
    context->Draw(6, 0);

    // 2. Dibujar Jugador
    DirectX::XMMATRIX playerWorld = DirectX::XMMatrixTranslation(scene.player.position.x, scene.player.position.y, scene.player.position.z);
    cb.mWorldViewProj = DirectX::XMMatrixTranspose(playerWorld * view * proj);
    context->UpdateSubresource(m_constantBuffer, 0, nullptr, &cb, 0, 0);

    context->IASetVertexBuffers(0, 1, &m_playerVB, &stride, &offset);
    context->Draw(3, 0); // Dibujamos el triangulo/cápsula placeholder
}

void Renderer3D::cleanup() {
    if(m_playerVB) m_playerVB->Release();
    if(m_floorVB) m_floorVB->Release();
    if(m_constantBuffer) m_constantBuffer->Release();
    if(m_inputLayout) m_inputLayout->Release();
    if(m_vertexShader) m_vertexShader->Release();
    if(m_pixelShader) m_pixelShader->Release();
}