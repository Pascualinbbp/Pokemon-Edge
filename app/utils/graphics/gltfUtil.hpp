#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../nlohmann/json.hpp"
#include "../core/loggerUtil.hpp"

// Lectura de modelos glTF 2.0 binarios (.glb): mallas, jerarquía de nodos, articulaciones (extras) y la textura incrustada.
// Solo trabaja en CPU (no sabe nada de DirectX 11): quien lo use crea los buffers de GPU.
// Convención: glTF es de mano derecha (Y arriba, X a la derecha, el frente es +Z); DirectX es de mano izquierda. Al cargar se refleja
// el eje X (posiciones, normales, matrices de los nodos y ejes de giro) y se invierte el orden de los triángulos, así el modelo se ve
// igual que en el editor, con su frente en +Z y la cara frontal de cada triángulo hacia fuera.
namespace GltfUtil {
    // Nombre de la clave de 'extras' donde la herramienta de exportación guarda cómo se mueve una pieza:
    //   "extras": { "janus": { "axis": [x, y, z], "open_angle_deg": grados } }
    inline constexpr const char* EXTRAS_KEY = "janus";

    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
    };

    struct Primitive {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    // Articulación de una pieza: gira sobre 'axis' (en el espacio local del nodo, pasando por su origen = el pivote) hasta 'maxAngle'.
    struct Joint {
        DirectX::XMFLOAT3 axis = { 1.0f, 0.0f, 0.0f };
        float maxAngle = 0.0f; // radianes, con su signo (regla de la mano derecha)
        bool valid = false;
    };

    struct Node {
        std::string name;
        int parent = -1;
        int mesh = -1;
        DirectX::XMFLOAT4X4 local; // de este nodo a su padre (convención de DirectXMath: vector fila)
        Joint joint;
    };

    struct Document {
        std::vector<Node> nodes;                       // el padre siempre aparece antes que sus hijos
        std::vector<std::vector<Primitive>> meshes;
        std::vector<unsigned char> image;              // primera imagen incrustada (PNG o JPG), vacía si no hay
        int sceneNodeCount = 0;
    };

    namespace detail {
        using json = nlohmann::json;

        struct Buffers {
            const unsigned char* bin = nullptr;
            size_t binSize = 0;
        };

        inline int componentSize(int type) {
            switch (type) { case 5120: case 5121: return 1; case 5122: case 5123: return 2; default: return 4; }
        }
        inline int componentCount(const std::string& type) {
            if (type == "SCALAR") return 1;
            if (type == "VEC2") return 2;
            if (type == "VEC3") return 3;
            if (type == "VEC4") return 4;
            return 0;
        }

        // Lee un accesorio como flotantes (o enteros convertidos). 'count' y 'components' son los del accesorio.
        inline bool readAccessor(const json& doc, const Buffers& buffers, int index, std::vector<float>& out, int& components) {
            if (index < 0 || !doc.contains("accessors") || index >= static_cast<int>(doc["accessors"].size())) return false;
            const json& accessor = doc["accessors"][index];
            const json& view = doc["bufferViews"][accessor.value("bufferView", 0)];
            components = componentCount(accessor.value("type", "SCALAR"));
            const int type = accessor.value("componentType", 5126);
            const size_t count = accessor.value("count", 0);
            const size_t size = static_cast<size_t>(componentSize(type));
            const size_t stride = view.contains("byteStride") ? view["byteStride"].get<size_t>() : size * components;
            const size_t start = view.value("byteOffset", 0u) + accessor.value("byteOffset", 0u);
            if (components == 0 || start + (count ? (count - 1) * stride + size * components : 0) > buffers.binSize) return false;

            out.resize(count * components);
            for (size_t i = 0; i < count; ++i) {
                for (int c = 0; c < components; ++c) {
                    const unsigned char* p = buffers.bin + start + i * stride + c * size;
                    float v = 0.0f;
                    switch (type) {
                        case 5126: { float f; std::memcpy(&f, p, 4); v = f; break; }
                        case 5125: { uint32_t u; std::memcpy(&u, p, 4); v = static_cast<float>(u); break; }
                        case 5123: { uint16_t u; std::memcpy(&u, p, 2); v = static_cast<float>(u); break; }
                        case 5121: v = static_cast<float>(*p); break;
                        default: return false;
                    }
                    out[i * components + c] = v;
                }
            }
            return true;
        }

        inline bool readPrimitive(const json& doc, const Buffers& buffers, const json& primitive, Primitive& out) {
            if (primitive.value("mode", 4) != 4 || !primitive.contains("attributes")) return false;
            const json& attributes = primitive["attributes"];
            std::vector<float> position, normal, uv, index;
            int pc = 0, nc = 0, uc = 0, ic = 0;
            if (!attributes.contains("POSITION") || !readAccessor(doc, buffers, attributes["POSITION"], position, pc) || pc != 3) return false;
            const bool hasNormal = attributes.contains("NORMAL") && readAccessor(doc, buffers, attributes["NORMAL"], normal, nc) && nc == 3;
            const bool hasUv = attributes.contains("TEXCOORD_0") && readAccessor(doc, buffers, attributes["TEXCOORD_0"], uv, uc) && uc == 2;

            const size_t count = position.size() / 3;
            out.vertices.resize(count);
            for (size_t i = 0; i < count; ++i) {
                out.vertices[i].position = { -position[i * 3], position[i * 3 + 1], position[i * 3 + 2] };
                out.vertices[i].normal = hasNormal ? DirectX::XMFLOAT3{ -normal[i * 3], normal[i * 3 + 1], normal[i * 3 + 2] } : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
                out.vertices[i].uv = hasUv ? DirectX::XMFLOAT2{ uv[i * 2], uv[i * 2 + 1] } : DirectX::XMFLOAT2{ 0.0f, 0.0f };
            }
            if (primitive.contains("indices") && readAccessor(doc, buffers, primitive["indices"], index, ic)) {
                out.indices.resize(index.size());
                for (size_t i = 0; i < index.size(); ++i) out.indices[i] = static_cast<uint32_t>(index[i]);
            } else {
                out.indices.resize(count);
                for (size_t i = 0; i < count; ++i) out.indices[i] = static_cast<uint32_t>(i);
            }
            out.indices.resize(out.indices.size() / 3 * 3);

            // Cambio de mano: se invierte el orden de cada triángulo.
            for (size_t i = 0; i + 2 < out.indices.size(); i += 3) std::swap(out.indices[i + 1], out.indices[i + 2]);

            if (!hasNormal) { // normales suaves a partir de las caras
                for (size_t i = 0; i + 2 < out.indices.size(); i += 3) {
                    const DirectX::XMVECTOR a = DirectX::XMLoadFloat3(&out.vertices[out.indices[i]].position);
                    const DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&out.vertices[out.indices[i + 1]].position);
                    const DirectX::XMVECTOR c = DirectX::XMLoadFloat3(&out.vertices[out.indices[i + 2]].position);
                    const DirectX::XMVECTOR n = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(b, a), DirectX::XMVectorSubtract(c, a));
                    for (int k = 0; k < 3; ++k) {
                        DirectX::XMFLOAT3& vn = out.vertices[out.indices[i + k]].normal;
                        DirectX::XMVECTOR sum = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&vn), n);
                        DirectX::XMStoreFloat3(&vn, sum);
                    }
                }
                for (Vertex& v : out.vertices) DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&v.normal)));
            }
            return !out.vertices.empty() && !out.indices.empty();
        }

        inline DirectX::XMFLOAT4X4 localMatrix(const json& node) {
            using namespace DirectX;
            XMFLOAT4X4 result;
            const XMMATRIX mirror = XMMatrixScaling(-1.0f, 1.0f, 1.0f); // glTF (mano derecha) -> DirectX (mano izquierda)
            if (node.contains("matrix") && node["matrix"].size() == 16) {
                float m[16];
                for (int i = 0; i < 16; ++i) m[i] = node["matrix"][i].get<float>();
                std::memcpy(&result, m, sizeof(m)); // columnas de glTF = filas de DirectXMath
                XMStoreFloat4x4(&result, mirror * XMLoadFloat4x4(&result) * mirror);
                return result;
            }
            XMFLOAT3 t = { 0, 0, 0 }, s = { 1, 1, 1 };
            XMFLOAT4 q = { 0, 0, 0, 1 };
            if (node.contains("translation")) t = { node["translation"][0].get<float>(), node["translation"][1].get<float>(), node["translation"][2].get<float>() };
            if (node.contains("scale")) s = { node["scale"][0].get<float>(), node["scale"][1].get<float>(), node["scale"][2].get<float>() };
            if (node.contains("rotation")) q = { node["rotation"][0].get<float>(), node["rotation"][1].get<float>(), node["rotation"][2].get<float>(), node["rotation"][3].get<float>() };
            const XMMATRIX m = XMMatrixScaling(s.x, s.y, s.z) * XMMatrixRotationQuaternion(XMLoadFloat4(&q)) * XMMatrixTranslation(t.x, t.y, t.z);
            XMStoreFloat4x4(&result, mirror * m * mirror);
            return result;
        }

        inline Joint readJoint(const json& node) {
            Joint joint;
            if (!node.contains("extras") || !node["extras"].contains(EXTRAS_KEY)) return joint;
            const json& data = node["extras"][EXTRAS_KEY];
            if (data.contains("axis") && data["axis"].size() == 3) {
                joint.axis = { data["axis"][0].get<float>(), -data["axis"][1].get<float>(), -data["axis"][2].get<float>() }; // eje reflejado (el ángulo no cambia)
            }
            joint.maxAngle = data.value("open_angle_deg", 0.0f) * 3.14159265f / 180.0f;
            joint.valid = std::fabs(joint.maxAngle) > 1.0e-5f;
            return joint;
        }

        // Añade el nodo y sus hijos (el padre antes que los hijos).
        inline void addNode(const json& doc, int index, int parent, Document& out, int depth = 0) {
            if (depth > 32 || index < 0 || index >= static_cast<int>(doc["nodes"].size())) return;
            const json& source = doc["nodes"][index];
            Node node;
            node.name = source.value("name", std::string());
            node.parent = parent;
            node.mesh = source.value("mesh", -1);
            node.local = localMatrix(source);
            node.joint = readJoint(source);
            const int self = static_cast<int>(out.nodes.size());
            out.nodes.push_back(node);
            if (source.contains("children")) for (const json& child : source["children"]) addNode(doc, child.get<int>(), self, out, depth + 1);
        }
    }

    inline std::optional<Document> parse(const unsigned char* data, size_t size) {
        const auto fail = [](const char* why) { Logger::logError("GLTF", why); return std::optional<Document>(); };
        if (!data || size < 20 || std::memcmp(data, "glTF", 4) != 0) return fail("No es un archivo .glb");

        // Cabecera (12 bytes) y trozos: JSON (0x4E4F534A) y binario (0x004E4942).
        const unsigned char* json = nullptr;
        size_t jsonSize = 0;
        detail::Buffers buffers;
        for (size_t offset = 12; offset + 8 <= size;) {
            uint32_t length, type;
            std::memcpy(&length, data + offset, 4);
            std::memcpy(&type, data + offset + 4, 4);
            if (offset + 8 + length > size) return fail("Trozo glb truncado");
            if (type == 0x4E4F534A) { json = data + offset + 8; jsonSize = length; }
            else if (type == 0x004E4942) { buffers.bin = data + offset + 8; buffers.binSize = length; }
            offset += 8 + ((length + 3u) & ~3u);
        }
        if (!json) return fail("El .glb no tiene JSON");

        detail::json doc = detail::json::parse(json, json + jsonSize, nullptr, false);
        if (doc.is_discarded() || !doc.contains("nodes") || !doc.contains("meshes")) return fail("JSON glTF no válido");

        Document out;
        for (const detail::json& mesh : doc["meshes"]) {
            std::vector<Primitive> primitives;
            for (const detail::json& source : mesh.value("primitives", detail::json::array())) {
                Primitive primitive;
                if (detail::readPrimitive(doc, buffers, source, primitive)) primitives.push_back(std::move(primitive));
            }
            out.meshes.push_back(std::move(primitives));
        }

        std::vector<int> roots;
        const int sceneIndex = doc.value("scene", 0);
        if (doc.contains("scenes") && sceneIndex < static_cast<int>(doc["scenes"].size())) {
            for (const detail::json& n : doc["scenes"][sceneIndex].value("nodes", detail::json::array())) roots.push_back(n.get<int>());
        } else {
            roots.push_back(0);
        }
        for (const int root : roots) detail::addNode(doc, root, -1, out);
        out.sceneNodeCount = static_cast<int>(out.nodes.size());

        if (doc.contains("images") && !doc["images"].empty() && doc["images"][0].contains("bufferView")) {
            const detail::json& view = doc["bufferViews"][doc["images"][0]["bufferView"].get<int>()];
            const size_t start = view.value("byteOffset", 0u), length = view.value("byteLength", 0u);
            if (start + length <= buffers.binSize) out.image.assign(buffers.bin + start, buffers.bin + start + length);
        }
        return out;
    }
}
