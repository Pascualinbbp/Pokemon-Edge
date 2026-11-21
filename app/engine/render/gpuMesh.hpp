#pragma once
#include <algorithm>
#include <cmath>
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

// Vértice común a todo lo que dibuja el renderizador (posición, normal y color). Los modelos con textura guardan en 'color' las
// coordenadas de la textura (x, y), si se les puede cambiar el color (z) y 1 (w), así comparten forma de vértice con todo lo demás.
struct GpuVertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 color;
};

// Geometría inmutable e indexada ya subida a la GPU.
struct GpuMesh {
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertices;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indices;
    UINT indexCount = 0;
    UINT stride = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R16_UINT;
    DirectX::XMFLOAT3 center = {};   // esfera envolvente en coordenadas de la malla (radio 0 = desconocida: nunca se descarta)
    float radius = 0.0f;

    // Calcula la esfera envolvente a partir de los vértices (la posición va al principio de cada uno).
    void bound(const void* data, size_t count, size_t strideBytes) {
        if (!data || count == 0 || strideBytes < sizeof(DirectX::XMFLOAT3)) return;
        const char* bytes = static_cast<const char*>(data);
        DirectX::XMFLOAT3 lo = { 1.0e9f, 1.0e9f, 1.0e9f }, hi = { -1.0e9f, -1.0e9f, -1.0e9f };
        for (size_t i = 0; i < count; ++i) {
            const DirectX::XMFLOAT3* p = reinterpret_cast<const DirectX::XMFLOAT3*>(bytes + i * strideBytes);
            lo = { (std::min)(lo.x, p->x), (std::min)(lo.y, p->y), (std::min)(lo.z, p->z) };
            hi = { (std::max)(hi.x, p->x), (std::max)(hi.y, p->y), (std::max)(hi.z, p->z) };
        }
        center = { (lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f };
        const float dx = hi.x - lo.x, dy = hi.y - lo.y, dz = hi.z - lo.z;
        radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz) + 0.01f;
    }
};
