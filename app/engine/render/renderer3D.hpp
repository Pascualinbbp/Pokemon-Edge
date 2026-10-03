#pragma once
#include <cstdint>
#include <vector>
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

    // Geometría inmutable e indexada: cada vértice se comparte entre triángulos.
    struct Mesh {
        ComPtr<ID3D11Buffer> vertices;
        ComPtr<ID3D11Buffer> indices;
        UINT indexCount = 0;
    };

    static Mesh createMesh(ID3D11Device* device, const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices);
    static Mesh createFloor(ID3D11Device* device);
    static Mesh createPlayer(ID3D11Device* device);
    void drawMesh(ID3D11DeviceContext* context, const Mesh& mesh,
        DirectX::CXMMATRIX world, DirectX::CXMMATRIX viewProj) const;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_constantBuffer;

    Mesh m_floor;
    Mesh m_player;

    // La proyección solo se recalcula cuando cambia el aspecto de la ventana.
    DirectX::XMFLOAT4X4 m_proj = {};
    float m_aspect = 0.0f;
};