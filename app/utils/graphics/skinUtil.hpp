#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>
#include "../nlohmann/json.hpp"
#include "gltfUtil.hpp"

// Animación de modelos con huesos, solo en CPU (no sabe nada de DirectX 11): lee los clips ("janus.animacion/1", un .json por animación
// con los giros y posiciones de cada hueso fotograma a fotograma), calcula la matriz de cada hueso en un instante y mueve los vértices.
// Los giros y posiciones del clip están en el espacio de glTF y son absolutos (sustituyen a los del hueso en reposo); aquí se reflejan
// igual que hace GltfUtil con los nodos (eje X), así el modelo animado se ve como en el editor.
namespace SkinUtil {
    // Giros y posiciones de un hueso, uno por fotograma (la posición puede faltar: el hueso conserva la de reposo).
    struct Track {
        std::vector<DirectX::XMFLOAT4> rot; // cuaternión x, y, z, w
        std::vector<DirectX::XMFLOAT3> pos;
    };

    struct Clip {
        float duration = 0.0f; // segundos
        float fps = 30.0f;
        int frames = 0;
        bool loop = false;
        std::unordered_map<std::string, Track> tracks; // por nombre de hueso
    };

    inline std::optional<Clip> parseClip(const unsigned char* data, size_t size) {
        using json = nlohmann::json;
        if (!data || size == 0) return std::nullopt;
        const json doc = json::parse(data, data + size, nullptr, false);
        if (doc.is_discarded() || !doc.contains("huesos") || !doc["huesos"].is_object()) return std::nullopt;
        Clip clip;
        clip.duration = doc.value("segundos", 0.0f);
        clip.fps = (std::max)(doc.value("fps", 30.0f), 1.0f);
        clip.frames = doc.value("fotogramas", 0);
        clip.loop = doc.value("bucle", false);
        for (auto it = doc["huesos"].begin(); it != doc["huesos"].end(); ++it) {
            Track track;
            if (it.value().contains("rot")) for (const json& q : it.value()["rot"]) if (q.size() == 4) track.rot.push_back({ q[0].get<float>(), q[1].get<float>(), q[2].get<float>(), q[3].get<float>() });
            if (it.value().contains("pos")) for (const json& p : it.value()["pos"]) if (p.size() == 3) track.pos.push_back({ p[0].get<float>(), p[1].get<float>(), p[2].get<float>() });
            clip.frames = (std::max)(clip.frames, static_cast<int>((std::max)(track.rot.size(), track.pos.size())));
            clip.tracks[it.key()] = std::move(track);
        }
        if (clip.frames <= 0) return std::nullopt;
        if (clip.duration <= 0.0f) clip.duration = static_cast<float>(clip.frames - 1) / clip.fps;
        return clip;
    }

    namespace detail {
        // Fotograma fraccionario de un clip en 'time' segundos (en bucle si el clip lo es).
        inline void frameAt(const Clip& clip, float time, int& a, int& b, float& t) {
            float frame = time * clip.fps;
            const float last = static_cast<float>(clip.frames - 1);
            if (clip.loop && last > 0.0f) frame = std::fmod(std::fmod(frame, last + 1.0f) + last + 1.0f, last + 1.0f);
            frame = (std::clamp)(frame, 0.0f, last);
            a = static_cast<int>(std::floor(frame));
            b = (std::min)(a + 1, clip.frames - 1);
            t = frame - static_cast<float>(a);
        }
    }

    // Matriz de cada nodo del documento (del nodo al modelo) con el clip en 'time' (nullptr: en reposo).
    inline void worlds(const GltfUtil::Document& doc, const Clip* clip, float time, std::vector<DirectX::XMFLOAT4X4>& out) {
        using namespace DirectX;
        out.resize(doc.nodes.size());
        int a = 0, b = 0;
        float t = 0.0f;
        if (clip) detail::frameAt(*clip, time, a, b, t);
        for (size_t i = 0; i < doc.nodes.size(); ++i) {
            const GltfUtil::Node& node = doc.nodes[i];
            XMMATRIX local = XMLoadFloat4x4(&node.local);
            const auto found = clip ? clip->tracks.find(node.name) : std::unordered_map<std::string, Track>::const_iterator();
            if (clip && found != clip->tracks.end()) {
                const Track& track = found->second;
                XMFLOAT3 translation = { node.local._41, node.local._42, node.local._43 }; // ya reflejada
                if (!track.pos.empty()) {
                    const XMFLOAT3& p0 = track.pos[(std::min)(static_cast<size_t>(a), track.pos.size() - 1)];
                    const XMFLOAT3& p1 = track.pos[(std::min)(static_cast<size_t>(b), track.pos.size() - 1)];
                    translation = { -(p0.x + (p1.x - p0.x) * t), p0.y + (p1.y - p0.y) * t, p0.z + (p1.z - p0.z) * t };
                }
                if (!track.rot.empty()) {
                    const XMFLOAT4& q0 = track.rot[(std::min)(static_cast<size_t>(a), track.rot.size() - 1)];
                    const XMFLOAT4& q1 = track.rot[(std::min)(static_cast<size_t>(b), track.rot.size() - 1)];
                    const XMVECTOR v0 = XMVectorSet(q0.x, -q0.y, -q0.z, q0.w); // giro reflejado en X
                    XMVECTOR v1 = XMVectorSet(q1.x, -q1.y, -q1.z, q1.w);
                    if (XMVectorGetX(XMVector4Dot(v0, v1)) < 0.0f) v1 = XMVectorNegate(v1); // camino corto
                    const XMVECTOR q = XMQuaternionNormalize(XMVectorLerp(v0, v1, t));
                    local = XMMatrixRotationQuaternion(q) * XMMatrixTranslation(translation.x, translation.y, translation.z);
                } else {
                    local.r[3] = XMVectorSet(translation.x, translation.y, translation.z, 1.0f);
                }
            }
            XMMATRIX world = local;
            if (node.parent >= 0) world = local * XMLoadFloat4x4(&out[static_cast<size_t>(node.parent)]);
            XMStoreFloat4x4(&out[i], world);
        }
    }

    // Matriz de piel de cada hueso de 'skin': de la malla en reposo a donde la lleva el hueso ahora.
    inline void skinMatrices(const GltfUtil::Document& doc, const GltfUtil::Skin& skin, const std::vector<DirectX::XMFLOAT4X4>& nodeWorlds, std::vector<DirectX::XMFLOAT4X4>& out) {
        using namespace DirectX;
        out.resize(skin.joints.size());
        for (size_t j = 0; j < skin.joints.size(); ++j) {
            const int node = skin.joints[j];
            XMStoreFloat4x4(&out[j], node >= 0 && node < static_cast<int>(nodeWorlds.size()) ? XMLoadFloat4x4(&skin.inverseBind[j]) * XMLoadFloat4x4(&nodeWorlds[static_cast<size_t>(node)]) : XMMatrixIdentity());
        }
        (void)doc;
    }

    // La parte de la piel que cuelga de un hueso concreto: 'tip' es el hueso del que sale y 'hidden' marca (por hueso de la piel) ese
    // hueso y todos sus descendientes.
    struct Split {
        int tip = -1;                // hueso de la piel
        std::vector<char> hidden;    // por hueso de la piel
        bool valid() const { return tip >= 0; }
    };

    // La cadena que el clip mueve con posiciones (el hilo y el corcho de una caña) empieza en el primer hueso con pista de posición.
    inline Split splitByPositionTrack(const GltfUtil::Document& doc, const GltfUtil::Skin& skin, const Clip& clip) {
        Split split;
        split.hidden.assign(skin.joints.size(), 0);
        int root = -1;
        for (size_t j = 0; j < skin.joints.size() && root < 0; ++j) {
            const int node = skin.joints[j];
            if (node < 0) continue;
            const auto found = clip.tracks.find(doc.nodes[static_cast<size_t>(node)].name);
            if (found != clip.tracks.end() && !found->second.pos.empty()) root = static_cast<int>(j);
        }
        if (root < 0) return split;
        const int rootNode = skin.joints[static_cast<size_t>(root)];
        const int parentNode = doc.nodes[static_cast<size_t>(rootNode)].parent;
        for (size_t j = 0; j < skin.joints.size(); ++j) {
            for (int n = skin.joints[j], depth = 0; n >= 0 && depth < 64; n = doc.nodes[static_cast<size_t>(n)].parent, ++depth) {
                if (n == rootNode) { split.hidden[j] = 1; break; }
            }
            if (skin.joints[j] == parentNode) split.tip = static_cast<int>(j);
        }
        return split;
    }

    // Mueve los vértices de una malla con las matrices de piel. Los vértices que cuelgan de 'split.hidden' (más de la mitad de su peso)
    // se recogen en el punto 'gather' (quedan sin superficie).
    inline void apply(const GltfUtil::Primitive& primitive, const std::vector<DirectX::XMFLOAT4X4>& matrices, std::vector<GltfUtil::Vertex>& out,
                      const Split* split = nullptr, const DirectX::XMFLOAT3* gather = nullptr) {
        using namespace DirectX;
        out.resize(primitive.vertices.size());
        std::vector<XMMATRIX> loaded(matrices.size());
        for (size_t i = 0; i < matrices.size(); ++i) loaded[i] = XMLoadFloat4x4(&matrices[i]);
        for (size_t v = 0; v < primitive.vertices.size(); ++v) {
            const GltfUtil::Vertex& source = primitive.vertices[v];
            if (v >= primitive.skin.size()) { out[v] = source; continue; }
            const GltfUtil::SkinWeights& s = primitive.skin[v];
            float hiddenWeight = 0.0f;
            XMVECTOR position = XMVectorZero(), normal = XMVectorZero();
            const XMVECTOR p = XMVectorSet(source.position.x, source.position.y, source.position.z, 1.0f);
            const XMVECTOR n = XMVectorSet(source.normal.x, source.normal.y, source.normal.z, 0.0f);
            for (int k = 0; k < 4; ++k) {
                if (s.weight[k] <= 0.0f || s.joint[k] >= loaded.size()) continue;
                if (split && s.joint[k] < split->hidden.size() && split->hidden[s.joint[k]]) hiddenWeight += s.weight[k];
                position = XMVectorAdd(position, XMVectorScale(XMVector3Transform(p, loaded[s.joint[k]]), s.weight[k]));
                normal = XMVectorAdd(normal, XMVectorScale(XMVector3TransformNormal(n, loaded[s.joint[k]]), s.weight[k]));
            }
            out[v] = source;
            if (gather && hiddenWeight > 0.5f) { out[v].position = *gather; continue; }
            XMStoreFloat3(&out[v].position, position);
            XMStoreFloat3(&out[v].normal, XMVector3Normalize(normal));
        }
    }
}
