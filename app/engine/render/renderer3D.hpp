#pragma once
#include <cstdint>
#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "../world/dayCycle.hpp"
#include "../world/scene.hpp"

// Render de la escena. Todo lo que hay en el mundo (paredes, jugador, pokémon, pokéballs...) se describe una
// sola vez como una lista de 'Draw'; esa misma lista se dibuja en el mapa de sombras y en la pantalla, así
// que todos los objetos proyectan y reciben sombras con exactamente la misma lógica.
class Renderer3D {
    public:
    void init(ID3D11Device* device);
    void render(ID3D11DeviceContext* context, const Scene& scene, int width, int height);
    void cleanup();

    // Proyección de la última imagen dibujada (la interfaz la usa para colocar los nombres sobre los pokémon).
    const DirectX::XMFLOAT4X4& projection() const { return m_proj; }

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
        DirectX::XMFLOAT4 tint; // rgb = color, a = brillo propio
    };

    // Datos de todo el frame: luz, sombras, ambiente y cielo (registro b1).
    struct FrameConstants {
        DirectX::XMFLOAT4X4 invSky; // inversa de (vista sin traslación * proyección)
        DirectX::XMFLOAT4X4 lightViewProj;
        DirectX::XMFLOAT4 lightDir;
        DirectX::XMFLOAT4 lightColor;
        DirectX::XMFLOAT4 ambientSky;
        DirectX::XMFLOAT4 ambientGround;
        DirectX::XMFLOAT4 sunDir;
        DirectX::XMFLOAT4 moonDir;
        DirectX::XMFLOAT4 skyZenith;
        DirectX::XMFLOAT4 skyHorizon;
        DirectX::XMFLOAT4 skyParams;    // x = estrellas, y = tinte cálido, z = ángulo del cielo, w = segundos
        DirectX::XMFLOAT4 shadowParams; // x = tamaño de texel, y = desplazamiento por la normal, z = sesgo de profundidad
    };

    // Geometría inmutable e indexada: cada vértice se comparte entre triángulos.
    struct Mesh {
        ComPtr<ID3D11Buffer> vertices;
        ComPtr<ID3D11Buffer> indices;
        UINT indexCount = 0;
        UINT stride = 0;
    };

    // Un objeto del mundo que proyecta y recibe sombras.
    struct Draw {
        const Mesh* mesh;
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 tint; // rgb = color que se multiplica a los vértices, a = brillo propio (0 = iluminado, 1 = sin sombreado)
        bool castsShadow;
    };

    static Mesh createMesh(ID3D11Device* device, const void* vertices, UINT vertexCount, UINT stride,
        const std::vector<uint16_t>& indices);
    static Mesh createFloor(ID3D11Device* device, const HabitatMap& habitats, const GameData& data);
    static Mesh createPlayer(ID3D11Device* device);
    static Mesh createCube(ID3D11Device* device);
    static Mesh createSphere(ID3D11Device* device, bool dome = false); // pokéball o, con dome, la semiesfera superior blanca
    static Mesh createStar(ID3D11Device* device);
    static DirectX::XMMATRIX lightViewProj(const DirectX::XMFLOAT3& lightDir);

    void collect(const Scene& scene);
    void add(const Mesh& mesh, DirectX::CXMMATRIX world, const DirectX::XMFLOAT4& tint, bool castsShadow = true);
    void addBall(const Pokeball& ball, float tilt, float scale, float glow);
    void addChest(const Chest& chest);
    void addNode(const ResourceNode& node);
    void addGroundItem(const GroundItem& item);
    void addMachine();
    void addCreature(const DirectX::XMFLOAT3& position, float size, float yaw, int speciesId, bool shiny);
    void setObject(ID3D11DeviceContext* context, DirectX::CXMMATRIX world, DirectX::CXMMATRIX worldViewProj,
        const DirectX::XMFLOAT4& tint) const;
    void drawIndexed(ID3D11DeviceContext* context, const Mesh& mesh) const;
    void drawAll(ID3D11DeviceContext* context, DirectX::CXMMATRIX viewProj, bool shadowPass) const;
    void renderShadowMap(ID3D11DeviceContext* context, DirectX::CXMMATRIX lightViewProj) const;
    void updateFrame(ID3D11DeviceContext* context, const DayCycle::Lighting& light, DirectX::CXMMATRIX view,
        DirectX::CXMMATRIX lightViewProj);

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11VertexShader> m_shadowVertexShader;
    ComPtr<ID3D11VertexShader> m_skyVertexShader;
    ComPtr<ID3D11PixelShader> m_skyPixelShader;
    ComPtr<ID3D11Buffer> m_objectBuffer;
    ComPtr<ID3D11Buffer> m_frameBuffer;
    ComPtr<ID3D11DepthStencilState> m_skyDepth;     // el cielo ignora la profundidad
    ComPtr<ID3D11DepthStencilView> m_shadowDsv;     // mapa de sombras (profundidad desde la luz)
    ComPtr<ID3D11ShaderResourceView> m_shadowSrv;
    ComPtr<ID3D11SamplerState> m_shadowSampler;     // comparación con filtrado: bordes suaves
    ComPtr<ID3D11RasterizerState> m_shadowRaster;   // con sesgo según la pendiente

    Mesh m_floor;                // suelo coloreado según los hábitats; se rehace cuando cambia el mapa
    ID3D11Device* m_device = nullptr;
    bool m_floorBuilt = false;
    unsigned m_floorSeed = 0;
    Mesh m_player;
    Mesh m_cube;   // cubo unitario blanco centrado en el origen: paredes, pokémon y su morro (se tiñe al dibujar)
    Mesh m_sphere; // pokéball: mitad tintada con el color del tipo, banda oscura y mitad blanca
    Mesh m_dome;   // semiesfera blanca de los objetos sueltos
    Mesh m_star;   // estrella de la animación de captura

    std::vector<Draw> m_draws; // se reutiliza entre frames

    // La proyección solo se recalcula cuando cambia el aspecto de la ventana o el campo de visión (apuntado).
    DirectX::XMFLOAT4X4 m_proj = {};
    float m_aspect = 0.0f;
    float m_fov = 0.0f;
};
