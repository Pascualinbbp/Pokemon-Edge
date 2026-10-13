#pragma once
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
};
