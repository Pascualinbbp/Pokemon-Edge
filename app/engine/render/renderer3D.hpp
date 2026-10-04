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

    // Vértice del disco de sombra: y = opacidad relativa (1 en el centro, 0 en el borde).
    struct ShadowVertex {
        DirectX::XMFLOAT3 pos;
    };

    // Datos por instancia de una sombra: centro (xyz), radio (w) y opacidad.
    struct ShadowInstance {
        DirectX::XMFLOAT4 positionRadius;
        float opacity;
    };

    // Geometría inmutable e indexada: cada vértice se comparte entre triángulos.
    struct Mesh {
        ComPtr<ID3D11Buffer> vertices;
        ComPtr<ID3D11Buffer> indices;
        UINT indexCount = 0;
        UINT stride = 0;
    };

    static Mesh createMesh(ID3D11Device* device, const void* vertices, UINT vertexCount, UINT stride,
        const std::vector<uint16_t>& indices);
    static Mesh createFloor(ID3D11Device* device);
    static Mesh createPlayer(ID3D11Device* device);
    static Mesh createCube(ID3D11Device* device);
    static Mesh createSphere(ID3D11Device* device);
    static Mesh createShadowDisc(ID3D11Device* device);

    void createShadowInstances(ID3D11Device* device, UINT capacity);
    void setTransform(ID3D11DeviceContext* context, DirectX::CXMMATRIX worldViewProj) const;
    void drawMesh(ID3D11DeviceContext* context, const Mesh& mesh,
        DirectX::CXMMATRIX world, DirectX::CXMMATRIX viewProj) const;
    void drawShadows(ID3D11DeviceContext* context, const Scene& scene, DirectX::CXMMATRIX viewProj);

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_constantBuffer;

    Mesh m_floor;
    Mesh m_player;
    Mesh m_cube;   // objetivo de capturas (cubo unitario centrado en el origen)
    Mesh m_sphere; // pokéball (esfera unitaria)

    // Sombras: un único disco dibujado con instancias (una sola llamada para todas).
    ComPtr<ID3D11VertexShader> m_shadowVertexShader;
    ComPtr<ID3D11PixelShader> m_shadowPixelShader;
    ComPtr<ID3D11InputLayout> m_shadowLayout;
    ComPtr<ID3D11BlendState> m_shadowBlend;
    ComPtr<ID3D11DepthStencilState> m_shadowDepth;
    ComPtr<ID3D11Buffer> m_shadowInstances;
    Mesh m_shadowDisc;
    UINT m_shadowCapacity = 0;
    std::vector<ShadowInstance> m_shadows; // se reutiliza cada frame

    // La proyección solo se recalcula cuando cambia el aspecto de la ventana o el campo de visión (apuntado).
    DirectX::XMFLOAT4X4 m_proj = {};
    float m_aspect = 0.0f;
    float m_fov = 0.0f;
};
