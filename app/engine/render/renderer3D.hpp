#pragma once
#include <cstdint>
#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "gpuMesh.hpp"
#include "model/model.hpp"
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

    using Vertex = GpuVertex;
    using Mesh = GpuMesh;

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
        DirectX::XMFLOAT4 cameraPos;    // xyz = posición de la cámara, w = marea (m)
        DirectX::XMFLOAT4 env;          // x = oscuridad (cueva, mina), y = densidad de la niebla, z = bajo el agua, w = radio del archipiélago
        DirectX::XMFLOAT4 fogColor;
    };

    // Entorno del frame que no sale de la luz del día: dónde está la cámara, la marea, la niebla y la oscuridad.
    struct Environment {
        DirectX::XMFLOAT3 eye = {};
        DirectX::XMFLOAT3 focus = {};   // punto que centra el mapa de sombras (el jugador)
        float tide = 0.0f;
        float darkness = 0.0f;
        bool underwater = false;
        float worldRadius = 300.0f;
    };

    // Cómo se dibuja un objeto: iluminado con el color de sus vértices, con la textura de un modelo, o brillo aditivo (rayos, destellos).
    enum class DrawKind { SOLID, MODEL, GLOW };

    // Un objeto del mundo que proyecta y recibe sombras.
    struct Draw {
        const Mesh* mesh;
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 tint; // rgb = color que se multiplica a los vértices, a = brillo propio (0 = iluminado, 1 = sin sombreado)
        bool castsShadow;
        DrawKind kind = DrawKind::SOLID;
        ID3D11ShaderResourceView* texture = nullptr; // solo MODEL
    };

    static Mesh createMesh(ID3D11Device* device, const void* vertices, UINT vertexCount, UINT stride,
        const std::vector<uint16_t>& indices);

    // Un trozo de malla del mundo (terreno o agua): se descarta entero si queda fuera del alcance de la cámara.
    struct Chunk {
        Mesh mesh;
        DirectX::XMFLOAT3 center = {};
        float radius = 0.0f;
    };
    template <typename VertexFn, typename KeepFn>
    static std::vector<Chunk> buildChunks(ID3D11Device* device, int nodes, VertexFn vertexAt, KeepFn keepCell);
    static std::vector<Chunk> createTerrain(ID3D11Device* device, const HabitatMap& habitats, const Physics::World& world, const GameData& data);
    static std::vector<Chunk> createWater(ID3D11Device* device, const HabitatMap& habitats, const Physics::World& world);
    static Mesh createOcean(ID3D11Device* device);
    static Mesh createCone(ID3D11Device* device);
    static Mesh createCube(ID3D11Device* device);
    static Mesh createSphere(ID3D11Device* device, bool dome = false); // pokéball o, con dome, la semiesfera superior blanca
    static Mesh createStar(ID3D11Device* device);
    static Mesh createBeam(ID3D11Device* device); // cono abierto a lo largo de +Z: estrecho en z = 0 (la bola) y ancho en z = 1 (el pokémon)
    static DirectX::XMMATRIX lightViewProj(const DirectX::XMFLOAT3& lightDir, const DirectX::XMFLOAT3& focus);

    void collect(const Scene& scene);
    void add(const Mesh& mesh, DirectX::CXMMATRIX world, const DirectX::XMFLOAT4& tint, bool castsShadow = true,
        DrawKind kind = DrawKind::SOLID, ID3D11ShaderResourceView* texture = nullptr);
    // Instancia de un modelo cargado: cada pieza con su apertura (ModelPose) y todo bajo la transformación 'world'.
    void addModel(const RenderModel& model, DirectX::CXMMATRIX world, const DirectX::XMFLOAT4& tint, const ModelPose& pose, bool castsShadow = true);
    void addBall(const DirectX::XMFLOAT3& position, float yaw, float spin, float roll, float lid, float scale, const DirectX::XMFLOAT3& color, float glow);
    void addBall(const Pokeball& ball, float scale = 1.0f) { addBall(ball.body.position, ball.yaw, ball.spin, 0.0f, 0.0f, scale, ball.color, 0.0f); }
    void addCapture(const CaptureSequence& sequence);
    void addGlowBetween(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to, float width, const DirectX::XMFLOAT4& tint);
    void addPlayer(const Scene& scene);
    void addTool(PlayerAction tool, DirectX::CXMMATRIX hand);
    void addFishingLine(const Scene& scene, const DirectX::XMFLOAT3& rodTip);
    void addSky(const Scene& scene, float time);
    void addChest(const Chest& chest);
    void addNode(const ResourceNode& node);
    void addGroundItem(const GroundItem& item);
    void addMachine();
    void addCreature(const DirectX::XMFLOAT3& position, float size, float yaw, int speciesId, bool shiny);
    void setObject(ID3D11DeviceContext* context, DirectX::CXMMATRIX world, DirectX::CXMMATRIX worldViewProj,
        const DirectX::XMFLOAT4& tint) const;
    void drawIndexed(ID3D11DeviceContext* context, const Mesh& mesh) const;
    void drawAll(ID3D11DeviceContext* context, DirectX::CXMMATRIX viewProj, bool shadowPass) const;
    void drawGlow(ID3D11DeviceContext* context, DirectX::CXMMATRIX viewProj) const;
    void drawChunks(ID3D11DeviceContext* context, const std::vector<Chunk>& chunks, DirectX::CXMMATRIX viewProj) const;
    void drawWater(ID3D11DeviceContext* context, DirectX::CXMMATRIX viewProj) const;
    void renderShadowMap(ID3D11DeviceContext* context, DirectX::CXMMATRIX lightViewProj) const;
    void updateFrame(ID3D11DeviceContext* context, const DayCycle::Lighting& light, DirectX::CXMMATRIX view,
        DirectX::CXMMATRIX lightViewProj, const Environment& env);

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
    ComPtr<ID3D11VertexShader> m_waterVertexShader; // agua: olas, marea, corriente
    ComPtr<ID3D11PixelShader> m_waterPixelShader;   // agua: transparencia por profundidad, espuma, reflejo
    ComPtr<ID3D11BlendState> m_waterBlend;
    ComPtr<ID3D11DepthStencilState> m_waterDepth;   // el agua prueba la profundidad pero no la escribe
    ComPtr<ID3D11RasterizerState> m_noCull;         // el agua se ve también desde abajo
    ComPtr<ID3D11PixelShader> m_modelPixelShader;   // modelos con textura
    ComPtr<ID3D11PixelShader> m_glowPixelShader;    // brillo aditivo
    ComPtr<ID3D11BlendState> m_additiveBlend;
    ComPtr<ID3D11SamplerState> m_modelSampler;

    std::vector<Chunk> m_terrain; // relieve coloreado según hábitat, altura y pendiente; se rehace cuando cambia el mapa
    std::vector<Chunk> m_water;   // superficie del agua (mar, ríos, lagos, charcas, cascadas)
    Mesh m_ocean;                 // el mar hasta el horizonte, fuera del archipiélago
    Mesh m_cone;                  // la base rocosa de la isla del cielo
    ID3D11Device* m_device = nullptr;
    bool m_worldBuilt = false;
    unsigned m_worldSeed = 0;
    DirectX::XMFLOAT3 m_eye = {}; // posición de la cámara en el último frame (recorte del terreno)
    float m_darkness = 0.0f;      // oscuridad actual (suave al entrar y salir de cuevas)

    Mesh m_cube;   // cubo unitario blanco centrado en el origen: paredes, pokémon y su morro (se tiñe al dibujar)
    Mesh m_sphere; // pokéball: mitad tintada con el color del tipo, banda oscura y mitad blanca
    Mesh m_dome;   // semiesfera blanca de los objetos sueltos
    Mesh m_star;   // estrella de la animación de captura
    Mesh m_beam;   // rayo rojo de la captura
    ModelCache m_models;                  // modelos .glb cargados una sola vez
    const RenderModel* m_ballModel = nullptr;
    int m_ballLid = -1;                   // pieza articulada de la pokéball (la tapa)

    std::vector<Draw> m_draws; // se reutiliza entre frames

    // La proyección solo se recalcula cuando cambia el aspecto de la ventana o el campo de visión (apuntado).
    DirectX::XMFLOAT4X4 m_proj = {};
    float m_aspect = 0.0f;
    float m_fov = 0.0f;
};
