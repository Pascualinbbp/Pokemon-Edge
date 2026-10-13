#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>
#include "../gpuMesh.hpp"
#include "../../../utils/core/pathsUtil.hpp"
#include "../../../utils/core/resourceUtil.hpp"
#include "../../../utils/graphics/gltfUtil.hpp"
#include "../../../utils/graphics/imageUtil.hpp"
#include "../../../utils/graphics/textureUtil.hpp"

// Una pieza de un modelo: su malla, dónde está en el modelo y, si se mueve, cómo (pivote = origen del nodo, eje y ángulo máximo).
struct ModelPart {
    std::string name;
    GpuMesh mesh;
    DirectX::XMFLOAT4X4 rest;  // del nodo al modelo, en reposo
    GltfUtil::Joint joint;
    bool animated() const { return joint.valid; }
};

// Modelo 3D listo para dibujar: sus piezas y su textura. No sabe nada de ningún objeto concreto del juego.
struct RenderModel {
    static constexpr int MAX_PARTS = 8; // piezas animables por instancia (ModelPose)

    std::vector<ModelPart> parts;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    DirectX::XMFLOAT3 boundsMin = {}, boundsMax = {};

    // Índice de la pieza con ese nombre (-1 si no existe).
    int part(const std::string& name) const {
        for (size_t i = 0; i < parts.size(); ++i) if (parts[i].name == name) return static_cast<int>(i);
        return -1;
    }
    DirectX::XMFLOAT3 center() const { return { (boundsMin.x + boundsMax.x) * 0.5f, (boundsMin.y + boundsMax.y) * 0.5f, (boundsMin.z + boundsMax.z) * 0.5f }; }
    float height() const { return boundsMax.y - boundsMin.y; }

    // Del modelo al mundo: la pieza con su articulación abierta 'fraction' (0 = reposo, 1 = ángulo máximo).
    DirectX::XMMATRIX partMatrix(int index, float fraction) const {
        const ModelPart& p = parts[index];
        DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&p.rest);
        if (p.joint.valid && fraction != 0.0f) {
            const DirectX::XMVECTOR axis = DirectX::XMLoadFloat3(&p.joint.axis);
            m = DirectX::XMMatrixRotationAxis(axis, p.joint.maxAngle * fraction) * m; // el origen del nodo es el pivote
        }
        return m;
    }
};

// Cuánto está abierta (0..1) cada pieza articulada de una instancia de un modelo.
struct ModelPose {
    float fraction[RenderModel::MAX_PARTS] = {};
    void set(int part, float value) { if (part >= 0 && part < RenderModel::MAX_PARTS) fraction[part] = value; }
    float get(int part) const { return part >= 0 && part < RenderModel::MAX_PARTS ? fraction[part] : 0.0f; }
};

// Caché de modelos: cada .glb se carga y se sube a la GPU una sola vez y todos los usos comparten el resultado.
class ModelCache {
    public:
    // 'name' es la carpeta/archivo dentro de assets/models (PathsUtil::modelAssetPath). nullptr si no se pudo cargar (se intenta una vez).
    // 'tintable': las zonas rojas de la textura toman el color con el que se dibuje (como las pokéballs de cada tipo).
    const RenderModel* get(ID3D11Device* device, const std::string& name, bool tintable = false) {
        const auto found = m_models.find(name);
        if (found != m_models.end()) return found->second.get();
        std::unique_ptr<RenderModel> model = load(device, name, tintable);
        const RenderModel* result = model.get();
        m_models[name] = std::move(model);
        return result;
    }

    void clear() { m_models.clear(); }

    private:
    static GpuMesh upload(ID3D11Device* device, const std::vector<GpuVertex>& vertices, const std::vector<uint32_t>& indices) {
        GpuMesh mesh;
        mesh.stride = sizeof(GpuVertex);
        mesh.indexCount = static_cast<UINT>(indices.size());
        mesh.vertices = D3dUtil::createBuffer(device, static_cast<UINT>(vertices.size() * sizeof(GpuVertex)), D3D11_BIND_VERTEX_BUFFER,
            vertices.data(), D3D11_USAGE_IMMUTABLE, "CreateBuffer (model vertices)");
        if (vertices.size() <= 65535) {
            std::vector<uint16_t> small(indices.begin(), indices.end());
            mesh.indices = D3dUtil::createBuffer(device, static_cast<UINT>(small.size() * sizeof(uint16_t)), D3D11_BIND_INDEX_BUFFER,
                small.data(), D3D11_USAGE_IMMUTABLE, "CreateBuffer (model indices)");
            mesh.indexFormat = DXGI_FORMAT_R16_UINT;
        } else {
            mesh.indices = D3dUtil::createBuffer(device, static_cast<UINT>(indices.size() * sizeof(uint32_t)), D3D11_BIND_INDEX_BUFFER,
                indices.data(), D3D11_USAGE_IMMUTABLE, "CreateBuffer (model indices)");
            mesh.indexFormat = DXGI_FORMAT_R32_UINT;
        }
        return mesh;
    }

    static std::unique_ptr<RenderModel> load(ID3D11Device* device, const std::string& name, bool tintable) {
        const ResourceUtil::Blob blob = ResourceUtil::loadAsset(PathsUtil::modelAssetPath(name));
        if (!blob) return nullptr;
        const std::optional<GltfUtil::Document> document = GltfUtil::parse(blob.data, blob.size);
        if (!document) return nullptr;

        auto model = std::make_unique<RenderModel>();
        std::vector<DirectX::XMFLOAT4X4> rest(document->nodes.size());
        DirectX::XMFLOAT3 lo = { 1.0e9f, 1.0e9f, 1.0e9f }, hi = { -1.0e9f, -1.0e9f, -1.0e9f };
        for (size_t i = 0; i < document->nodes.size(); ++i) {
            const GltfUtil::Node& node = document->nodes[i];
            DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&node.local);
            if (node.parent >= 0) m = m * DirectX::XMLoadFloat4x4(&rest[node.parent]);
            DirectX::XMStoreFloat4x4(&rest[i], m);
            if (node.mesh < 0 || node.mesh >= static_cast<int>(document->meshes.size())) continue;

            for (const GltfUtil::Primitive& primitive : document->meshes[node.mesh]) {
                std::vector<GpuVertex> vertices(primitive.vertices.size());
                for (size_t v = 0; v < vertices.size(); ++v) {
                    const GltfUtil::Vertex& s = primitive.vertices[v];
                    vertices[v] = { s.position, s.normal, { s.uv.x, s.uv.y, tintable ? 1.0f : 0.0f, 1.0f } };
                    DirectX::XMFLOAT3 world;
                    DirectX::XMStoreFloat3(&world, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&s.position), m));
                    lo = { (std::min)(lo.x, world.x), (std::min)(lo.y, world.y), (std::min)(lo.z, world.z) };
                    hi = { (std::max)(hi.x, world.x), (std::max)(hi.y, world.y), (std::max)(hi.z, world.z) };
                }
                ModelPart part;
                part.name = node.name;
                part.mesh = upload(device, vertices, primitive.indices);
                part.rest = rest[i];
                part.joint = node.joint;
                model->parts.push_back(std::move(part));
            }
        }
        if (model->parts.empty()) return nullptr;
        model->boundsMin = lo;
        model->boundsMax = hi;

        if (!document->image.empty()) {
            if (const std::optional<ImageUtil::Image> image = ImageUtil::decode(document->image)) model->texture = TextureUtil::createFromImage(device, *image);
        }
        return model;
    }

    std::unordered_map<std::string, std::unique_ptr<RenderModel>> m_models;
};
