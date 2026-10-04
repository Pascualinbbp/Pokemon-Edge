#pragma once
#include <cstdint>
#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "../world/dayCycle.hpp"
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
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 color;
    };

    // Datos por objeto (registro b0). Se envían traspuestos.
    struct ObjectConstants {
        DirectX::XMFLOAT4X4 worldViewProj;
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 tint; // a = opacidad de la sombra
    };

    // Datos de todo el frame: luz, ambiente y cielo (registro b1).
    struct FrameConstants {
        DirectX::XMFLOAT4X4 invSky; // inversa de (vista sin traslación * proyección)
        DirectX::XMFLOAT4 lightDir;
        DirectX::XMFLOAT4 lightColor;
        DirectX::XMFLOAT4 ambientSky;
        DirectX::XMFLOAT4 ambientGround;
        DirectX::XMFLOAT4 sunDir;
        DirectX::XMFLOAT4 moonDir;
        DirectX::XMFLOAT4 skyZenith;
        DirectX::XMFLOAT4 skyHorizon;
        DirectX::XMFLOAT4 skyParams; // x = estrellas, y = tinte cálido, z = ángulo del cielo, w = segundos
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
    static Mesh createCube(ID3D11Device* device, const DirectX::XMFLOAT3& tint);
    static Mesh createSphere(ID3D11Device* device);

    void setObject(ID3D11DeviceContext* context, DirectX::CXMMATRIX world, DirectX::CXMMATRIX worldViewProj, float alpha) const;
    void drawIndexed(ID3D11DeviceContext* context, const Mesh& mesh) const;
    void drawMesh(ID3D11DeviceContext* context, const Mesh& mesh, DirectX::CXMMATRIX world, DirectX::CXMMATRIX viewProj) const;
    void drawShadow(ID3D11DeviceContext* context, const Mesh& mesh, DirectX::CXMMATRIX world,
        DirectX::CXMMATRIX shadowViewProj, float opacity) const;
    void updateFrame(ID3D11DeviceContext* context, const DayCycle::Lighting& light, DirectX::CXMMATRIX view);

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11VertexShader> m_shadowVertexShader;
    ComPtr<ID3D11PixelShader> m_shadowPixelShader;
    ComPtr<ID3D11VertexShader> m_skyVertexShader;
    ComPtr<ID3D11PixelShader> m_skyPixelShader;
    ComPtr<ID3D11Buffer> m_objectBuffer;
    ComPtr<ID3D11Buffer> m_frameBuffer;
    ComPtr<ID3D11BlendState> m_shadowBlend;
    ComPtr<ID3D11DepthStencilState> m_shadowDepth; // prueba de profundidad sin escribirla
    ComPtr<ID3D11DepthStencilState> m_skyDepth;    // el cielo ignora la profundidad

    Mesh m_floor;
    Mesh m_player;
    Mesh m_cube;   // cuerpo del pokémon de pruebas (cubo unitario centrado en el origen)
    Mesh m_nose;   // cubito amarillo que marca hacia dónde mira
    Mesh m_sphere; // pokéball (esfera unitaria)

    // La proyección solo se recalcula cuando cambia el aspecto de la ventana o el campo de visión (apuntado).
    DirectX::XMFLOAT4X4 m_proj = {};
    float m_aspect = 0.0f;
    float m_fov = 0.0f;
};
