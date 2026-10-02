#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "../world/scene.hpp"

class Renderer3D {
    public:
    void init(ID3D11Device* device);
    void render(ID3D11DeviceContext* context, const Scene& scene, int width, int height);
    void cleanup();
    
    private:
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    
    struct Vertex {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
    };
    
    struct Mesh {
        ComPtr<ID3D11Buffer> buffer;
        UINT vertexCount = 0;
    };
    
    static Mesh createMesh(ID3D11Device* device, const Vertex* vertices, UINT count);
    void drawMesh(ID3D11DeviceContext* context, const Mesh& mesh,
        DirectX::CXMMATRIX world, DirectX::CXMMATRIX viewProj) const;
        
        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader> m_pixelShader;
        ComPtr<ID3D11InputLayout> m_inputLayout;
        ComPtr<ID3D11Buffer> m_constantBuffer;
        
        Mesh m_floor;
        Mesh m_player;
    };