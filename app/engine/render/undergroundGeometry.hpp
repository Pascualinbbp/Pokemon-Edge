#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <DirectXMath.h>
#include "gpuMesh.hpp"
#include "../world/habitat/habitatMap.hpp"
#include "../world/style/habitatStyle.hpp"
#include "../../models/gameData.hpp"

// Mallas del subsuelo (cuevas y minas) a partir de las capas de Underground: suelo, techo y paredes de cada galería, con el color del
// hábitat de cada punto. Solo geometría (sin Direct3D): el renderizador sube cada Batch a la GPU. Una galería es lo que hay entre el
// suelo y el techo de cada nodo; sus bordes son paredes. En la boca (celdas "hole") no hay techo: el cielo está abierto y el terreno
// no se dibuja, así que el suelo hace de suelo y, donde la roca de encima empieza, un dintel cierra el hueco entre techo y terreno.
namespace UndergroundGeometry {
    struct Batch {
        std::vector<GpuVertex> vertices;
        std::vector<uint16_t> indices;
        DirectX::XMFLOAT3 center = {};
        float radius = 0.0f;
    };

    inline constexpr size_t MAX_VERTICES = 65000;

    class Builder {
        public:
        Builder(const HabitatMap& habitats, const GameData& data) : m_under(habitats.under()), m_terrain(habitats.ground()) {
            for (const Habitat& habitat : data.habitats) m_colors.push_back(HabitatStyle::color(habitat.name));
        }

        std::vector<Batch> build() {
            for (const Underground::System& system : m_under.systems()) addSystem(system);
            flush();
            return std::move(m_batches);
        }

        private:
        using Float3 = DirectX::XMFLOAT3;

        static float mixf(float a, float b, float t) { return a + (b - a) * t; }
        static Float3 mix(const Float3& a, const Float3& b, float t) { return { mixf(a.x, b.x, t), mixf(a.y, b.y, t), mixf(a.z, b.z, t) }; }

        Float3 habitatColor(int col, int row) const {
            const int h = m_under.habitatAtNode(col, row);
            return h >= 0 && h < static_cast<int>(m_colors.size()) ? m_colors[h] : Float3{ 0.34f, 0.32f, 0.30f };
        }

        // Variación fina del tono, fija por nodo, para que la roca no sea lisa.
        static float grain(float x, float y, float z) { return 0.9f + 0.1f * std::sin(x * 1.7f + z * 0.9f) * std::cos(y * 2.3f + x * 0.7f); }

        // Normal de una capa en un nodo, por diferencias centradas (o de un solo lado junto al borde de la galería).
        Float3 normalOf(const Physics::Heightfield& field, int col, int row, float up) const {
            const float here = field.at(col, row);
            const auto sample = [&](int c, int r, int& used) { if (!m_under.nodeDefined(c, r)) return here; ++used; return field.at(c, r); };
            int usedX = 0, usedZ = 0;
            const float left = sample(col - 1, row, usedX), right = sample(col + 1, row, usedX);
            const float back = sample(col, row - 1, usedZ), front = sample(col, row + 1, usedZ);
            const float gx = usedX > 0 ? (right - left) / (Physics::Heightfield::CELL * usedX) : 0.0f;
            const float gz = usedZ > 0 ? (front - back) / (Physics::Heightfield::CELL * usedZ) : 0.0f;
            const float length = std::sqrt(gx * gx + 1.0f + gz * gz);
            return { -gx / length * up, up / length, -gz / length * up };
        }

        // Vértice nuevo en el lote actual; devuelve su índice.
        int vertex(const Float3& pos, const Float3& normal, const Float3& color) {
            m_batch.vertices.push_back({ pos, normal, { color.x, color.y, color.z, 1.0f } });
            return static_cast<int>(m_batch.vertices.size()) - 1;
        }

        // Triángulo con la cara frontal hacia 'normal' (cara frontal = cruz(b - a, c - a) hacia fuera, como el terreno).
        void triangle(int a, int b, int c, const Float3& normal) {
            const GpuVertex& va = m_batch.vertices[a];
            const GpuVertex& vb = m_batch.vertices[b];
            const GpuVertex& vc = m_batch.vertices[c];
            const DirectX::XMVECTOR cross = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&vb.pos), DirectX::XMLoadFloat3(&va.pos)),
                                                                   DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&vc.pos), DirectX::XMLoadFloat3(&va.pos)));
            const bool facing = DirectX::XMVectorGetX(DirectX::XMVector3Dot(cross, DirectX::XMLoadFloat3(&normal))) >= 0.0f;
            m_batch.indices.push_back(static_cast<uint16_t>(a));
            m_batch.indices.push_back(static_cast<uint16_t>(facing ? b : c));
            m_batch.indices.push_back(static_cast<uint16_t>(facing ? c : b));
        }

        void flush() {
            if (m_batch.indices.empty()) { m_batch = {}; return; }
            Float3 lo = { 1.0e9f, 1.0e9f, 1.0e9f }, hi = { -1.0e9f, -1.0e9f, -1.0e9f };
            for (const GpuVertex& v : m_batch.vertices) {
                lo = { (std::min)(lo.x, v.pos.x), (std::min)(lo.y, v.pos.y), (std::min)(lo.z, v.pos.z) };
                hi = { (std::max)(hi.x, v.pos.x), (std::max)(hi.y, v.pos.y), (std::max)(hi.z, v.pos.z) };
            }
            m_batch.center = { (lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f };
            m_batch.radius = 0.5f * std::sqrt((hi.x - lo.x) * (hi.x - lo.x) + (hi.y - lo.y) * (hi.y - lo.y) + (hi.z - lo.z) * (hi.z - lo.z));
            m_batches.push_back(std::move(m_batch));
            m_batch = {};
        }

        // Cara del suelo o del techo de una celda: cuatro vértices con la normal suavizada de cada nodo.
        void surface(const Physics::Heightfield& field, int col, int row, float up, float shade) {
            const int cols[4] = { col, col + 1, col, col + 1 }, rows[4] = { row, row, row + 1, row + 1 };
            int index[4];
            for (int i = 0; i < 4; ++i) {
                const float y = field.at(cols[i], rows[i]);
                const float x = field.nodeCoord(cols[i]), z = field.nodeCoord(rows[i]);
                Float3 color = habitatColor(cols[i], rows[i]);
                const bool wet = up > 0.0f && m_under.waterField().at(cols[i], rows[i]) - y > 0.2f;
                if (wet) color = mix(color, { 0.20f, 0.22f, 0.24f }, 0.6f); // el fondo de las charcas es cieno oscuro
                const float k = shade * grain(x, y, z);
                index[i] = vertex({ x, y, z }, normalOf(field, cols[i], rows[i], up), { color.x * k, color.y * k, color.z * k });
            }
            const Float3 n = { 0.0f, up, 0.0f };
            triangle(index[0], index[2], index[1], n);
            triangle(index[1], index[2], index[3], n);
        }

        // Pared vertical entre dos nodos contiguos, de 'bottom' a 'top' en cada uno, con la cara hacia 'normal' (horizontal).
        void wall(int c0, int r0, int c1, int r1, const float bottom[2], const float top[2], const Float3& normal) {
            if (top[0] - bottom[0] < 0.02f && top[1] - bottom[1] < 0.02f) return;
            const int cols[2] = { c0, c1 }, rows[2] = { r0, r1 };
            int index[4];
            for (int i = 0; i < 2; ++i) {
                const float x = m_terrain.nodeCoord(cols[i]), z = m_terrain.nodeCoord(rows[i]);
                const Float3 rock = mix(habitatColor(cols[i], rows[i]), { 0.40f, 0.37f, 0.34f }, 0.55f);
                const float k = grain(x, bottom[i], z);
                index[i] = vertex({ x, bottom[i], z }, normal, { rock.x * k * 0.55f, rock.y * k * 0.55f, rock.z * k * 0.55f });
                index[2 + i] = vertex({ x, (std::max)(top[i], bottom[i]), z }, normal, { rock.x * k * 0.75f, rock.y * k * 0.75f, rock.z * k * 0.75f });
            }
            triangle(index[0], index[1], index[2], normal);
            triangle(index[1], index[3], index[2], normal);
        }

        void addSystem(const Underground::System& system) {
            const Physics::Heightfield& floor = m_under.floorField();
            const int n = floor.count();
            const auto cellOf = [&](float v) { return (std::clamp)(static_cast<int>((v + floor.half()) / Physics::Heightfield::CELL), 0, n - 2); };
            const int c0 = (std::max)(cellOf(system.minX) - 2, 0), c1 = (std::min)(cellOf(system.maxX) + 2, n - 2);
            const int r0 = (std::max)(cellOf(system.minZ) - 2, 0), r1 = (std::min)(cellOf(system.maxZ) + 2, n - 2);
            const Physics::Heightfield& ceil = m_under.ceilField();
            for (int row = r0; row <= r1; ++row) {
                for (int col = c0; col <= c1; ++col) {
                    if (!m_under.cellDefined(col, row)) continue;
                    if (m_batch.vertices.size() + 64 > MAX_VERTICES) flush();
                    const bool open = m_under.hole(col, row);
                    surface(floor, col, row, 1.0f, 1.0f);
                    if (!open) surface(ceil, col, row, -1.0f, 0.62f);

                    // Las cuatro aristas: pared si el vecino no es galería; dintel si el vecino es la boca abierta y esta celda está cubierta.
                    struct Edge { int dc, dr, ca, ra, cb, rb; Float3 inward; };
                    const Edge edges[4] = {
                        { 0, -1, col, row, col + 1, row, { 0.0f, 0.0f, 1.0f } },
                        { 0, 1, col, row + 1, col + 1, row + 1, { 0.0f, 0.0f, -1.0f } },
                        { -1, 0, col, row, col, row + 1, { 1.0f, 0.0f, 0.0f } },
                        { 1, 0, col + 1, row, col + 1, row + 1, { -1.0f, 0.0f, 0.0f } },
                    };
                    for (const Edge& e : edges) {
                        const bool neighbour = m_under.cellDefined(col + e.dc, row + e.dr);
                        const float bottom[2] = { floor.at(e.ca, e.ra), floor.at(e.cb, e.rb) };
                        if (!neighbour) {
                            const float top[2] = { open ? m_terrain.at(e.ca, e.ra) : ceil.at(e.ca, e.ra), open ? m_terrain.at(e.cb, e.rb) : ceil.at(e.cb, e.rb) };
                            wall(e.ca, e.ra, e.cb, e.rb, bottom, top, e.inward);
                        } else if (!open && m_under.hole(col + e.dc, row + e.dr)) {
                            const float low[2] = { ceil.at(e.ca, e.ra), ceil.at(e.cb, e.rb) };
                            const float top[2] = { m_terrain.at(e.ca, e.ra), m_terrain.at(e.cb, e.rb) };
                            wall(e.ca, e.ra, e.cb, e.rb, low, top, { -e.inward.x, 0.0f, -e.inward.z });
                        }
                    }
                }
            }
        }

        const Underground& m_under;
        const Physics::Heightfield& m_terrain;
        std::vector<Float3> m_colors;
        Batch m_batch;
        std::vector<Batch> m_batches;
    };

    inline std::vector<Batch> build(const HabitatMap& habitats, const GameData& data) { return Builder(habitats, data).build(); }
}
