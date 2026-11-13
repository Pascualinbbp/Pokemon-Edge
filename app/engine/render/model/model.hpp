#pragma once
#include <cstring>
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
#include "../../../utils/graphics/skinUtil.hpp"
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

    // Modelos con huesos (una caña): su esqueleto, las mallas que mueve y las animaciones ya leídas. Sus mallas tienen el vértice dinámico y se
    // reescriben con pose(); el estado de la última pose es compartido por todos los usos del modelo, por eso solo se debe dibujar uno a la vez.
    struct SkinPart {
        int part = 0;                      // índice en 'parts'
        const GltfUtil::Primitive* source = nullptr;
        int skin = 0;
        std::vector<GpuVertex> base;       // vértices en reposo (conservan uv y tinte)
    };
    std::shared_ptr<GltfUtil::Document> skeleton;
    std::vector<SkinPart> skinParts;
    std::unordered_map<std::string, std::unique_ptr<SkinUtil::Clip>> clips; // nullptr si no existe
    bool skinned() const { return skeleton && !skinParts.empty(); }

    // Pone el modelo en el instante 'time' de 'clip' (nullptr: reposo). Con 'gatherLine' el hilo y el corcho que el clip mueve se recogen en la
    // punta de la caña (el juego dibuja los suyos). 'tip' recibe la punta, en coordenadas del modelo.
    bool pose(ID3D11DeviceContext* context, const SkinUtil::Clip* clip, float time, bool gatherLine, DirectX::XMFLOAT3* tip = nullptr) const {
        if (!skinned() || !context) return false;
        std::vector<DirectX::XMFLOAT4X4> worlds, matrices;
        SkinUtil::worlds(*skeleton, clip, time, worlds);
        SkinUtil::Split split;
        DirectX::XMFLOAT3 gather = {};
        const GltfUtil::Skin& firstSkin = skeleton->skins[skinParts.front().skin];
        if (clip) split = SkinUtil::splitByPositionTrack(*skeleton, firstSkin, *clip);
        if (split.valid() && firstSkin.joints[split.tip] >= 0) {
            const DirectX::XMFLOAT4X4& w = worlds[static_cast<size_t>(firstSkin.joints[split.tip])];
            gather = { w._41, w._42, w._43 };
            if (tip) *tip = gather;
        }
        std::vector<GltfUtil::Vertex> moved;
        for (const SkinPart& sp : skinParts) {
            SkinUtil::skinMatrices(*skeleton, skeleton->skins[sp.skin], worlds, matrices);
            SkinUtil::apply(*sp.source, matrices, moved, gatherLine && split.valid() ? &split : nullptr, &gather);
            std::vector<GpuVertex> out = sp.base;
            for (size_t v = 0; v < out.size() && v < moved.size(); ++v) { out[v].pos = moved[v].position; out[v].normal = moved[v].normal; }
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (SUCCEEDED(context->Map(parts[sp.part].mesh.vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                std::memcpy(mapped.pData, out.data(), out.size() * sizeof(GpuVertex));
                context->Unmap(parts[sp.part].mesh.vertices.Get(), 0);
            }
        }
        return true;
    }

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

    // Animación 'clip' de un modelo con huesos (assets/models/<name>_<clip>.json); nullptr si no existe.
    const SkinUtil::Clip* clip(const std::string& name, const std::string& clip) {
        const auto found = m_models.find(name);
        if (found == m_models.end() || !found->second) return nullptr;
        RenderModel& model = *found->second;
        const auto cached = model.clips.find(clip);
        if (cached != model.clips.end()) return cached->second.get();
        std::unique_ptr<SkinUtil::Clip> loaded;
        if (const ResourceUtil::Blob blob = ResourceUtil::loadAsset(PathsUtil::modelClipPath(name, clip))) {
            if (std::optional<SkinUtil::Clip> parsed = SkinUtil::parseClip(blob.data, blob.size)) loaded = std::make_unique<SkinUtil::Clip>(std::move(*parsed));
        }
        const SkinUtil::Clip* result = loaded.get();
        model.clips[clip] = std::move(loaded);
        return result;
    }

    void clear() { m_models.clear(); }

    private:
    static GpuMesh upload(ID3D11Device* device, const std::vector<GpuVertex>& vertices, const std::vector<uint32_t>& indices, bool dynamic = false) {
        GpuMesh mesh;
        mesh.stride = sizeof(GpuVertex);
        mesh.indexCount = static_cast<UINT>(indices.size());
        mesh.vertices = D3dUtil::createBuffer(device, static_cast<UINT>(vertices.size() * sizeof(GpuVertex)), D3D11_BIND_VERTEX_BUFFER,
            vertices.data(), dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE, "CreateBuffer (model vertices)");
        if (vertices.size() <= 65535) {
            std::vector<uint16_t> narrow(indices.begin(), indices.end());
            mesh.indices = D3dUtil::createBuffer(device, static_cast<UINT>(narrow.size() * sizeof(uint16_t)), D3D11_BIND_INDEX_BUFFER,
                narrow.data(), D3D11_USAGE_IMMUTABLE, "CreateBuffer (model indices)");
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
        const auto shared = std::make_shared<GltfUtil::Document>(*document);
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
                const bool skinned = node.skin >= 0 && node.skin < static_cast<int>(shared->skins.size()) && primitive.skin.size() == primitive.vertices.size();
                ModelPart part;
                part.name = node.name;
                part.mesh = upload(device, vertices, primitive.indices, skinned);
                part.rest = rest[i];
                if (skinned) { // los vértices ya están en el espacio del modelo: la piel sustituye al nodo de la malla
                    DirectX::XMStoreFloat4x4(&part.rest, DirectX::XMMatrixIdentity());
                    RenderModel::SkinPart sp;
                    sp.part = static_cast<int>(model->parts.size());
                    sp.source = &shared->meshes[node.mesh][&primitive - document->meshes[node.mesh].data()];
                    sp.skin = node.skin;
                    sp.base = vertices;
                    model->skinParts.push_back(std::move(sp));
                }
                part.joint = node.joint;
                model->parts.push_back(std::move(part));
            }
        }
        if (model->parts.empty()) return nullptr;
        if (!model->skinParts.empty()) model->skeleton = shared;
        model->boundsMin = lo;
        model->boundsMax = hi;

        if (!document->image.empty()) {
            if (const std::optional<ImageUtil::Image> image = ImageUtil::decode(document->image)) model->texture = TextureUtil::createFromImage(device, *image);
        }
        return model;
    }

    std::unordered_map<std::string, std::unique_ptr<RenderModel>> m_models;
};
