#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "scene.hpp"

class Renderer3D {
public:
    void init(ID3D11Device* device);
    void render(ID3D11DeviceContext* context, const Scene& scene, int width, int height);
    void cleanup();

private:
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
    ID3D11Buffer* m_constantBuffer = nullptr;
    
    // Geometry
    ID3D11Buffer* m_playerVB = nullptr;
    ID3D11Buffer* m_floorVB = nullptr;
};