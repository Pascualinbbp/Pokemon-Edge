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

        // Ruido de valor 3D suave (0..1), fijo por posición: la roca de dos paredes que se tocan coincide en el borde común.
        static float hash3(int x, int y, int z) {
            uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + static_cast<uint32_t>(z) * 2147483647u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
        }
        static float rockNoise(float x, float y, float z) {
            const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y)), iz = static_cast<int>(std::floor(z));
            const float fx = x - ix, fy = y - iy, fz = z - iz;
            const float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy), sz = fz * fz * (3.0f - 2.0f * fz);
            float v = 0.0f;
            for (int k = 0; k < 8; ++k) {
                const int dx = k & 1, dy = (k >> 1) & 1, dz = (k >> 2) & 1;
                v += hash3(ix + dx, iy + dy, iz + dz) * (dx ? sx : 1.0f - sx) * (dy ? sy : 1.0f - sy) * (dz ? sz : 1.0f - sz);
            }
            return v;
        }

        // Pared entre dos nodos contiguos, de 'bottom' a 'top' en cada uno, con la cara hacia 'normal' (horizontal). Es una malla fina cuya
        // superficie se abomba hacia la roca con ruido (más en el centro de cada tramo y a media altura, nada en los bordes, que enlazan con el
        // suelo, el techo y las paredes vecinas): la piedra parece natural y no hay ángulos rectos. Nunca se mete en la galería, así que no
        // cambia por dónde se puede andar. 'rough' = 0 la deja plana (dinteles).
        void wall(int c0, int r0, int c1, int r1, const float bottom[2], const float top[2], const Float3& normal, float rough = 1.0f) {
            if (top[0] - bottom[0] < 0.02f && top[1] - bottom[1] < 0.02f) return;
            constexpr int NU = 4, NV = 4;
            const float x0 = m_terrain.nodeCoord(c0), z0 = m_terrain.nodeCoord(r0), x1 = m_terrain.nodeCoord(c1), z1 = m_terrain.nodeCoord(r1);
            const Float3 rockA = mix(habitatColor(c0, r0), { 0.40f, 0.37f, 0.34f }, 0.55f), rockB = mix(habitatColor(c1, r1), { 0.40f, 0.37f, 0.34f }, 0.55f);
            Float3 pos[NU + 1][NV + 1];
            float bulge[NU + 1][NV + 1];
            for (int i = 0; i <= NU; ++i) {
                const float u = static_cast<float>(i) / NU;
                const float x = mixf(x0, x1, u), z = mixf(z0, z1, u);
                const float lo = mixf(bottom[0], bottom[1], u), hi = (std::max)(mixf(top[0], top[1], u), lo);
                for (int j = 0; j <= NV; ++j) {
                    const float v = static_cast<float>(j) / NV, y = mixf(lo, hi, v);
                    const float edge = std::pow((std::max)(0.0f, std::sin(3.14159265f * u)), 0.7f) * std::pow((std::max)(0.0f, std::sin(3.14159265f * v)), 0.55f);
                    const float n = 0.55f * rockNoise(x * 0.9f, y * 0.9f, z * 0.9f) + 0.45f * rockNoise(x * 2.3f + 7.0f, y * 2.3f, z * 2.3f + 3.0f);
                    const float amount = rough * edge * (0.15f + 1.35f * n) * (std::min)(1.0f, 0.5f + 0.25f * (hi - lo));
                    bulge[i][j] = amount;
                    pos[i][j] = { x - normal.x * amount, y, z - normal.z * amount };
                }
            }
            int index[NU + 1][NV + 1];
            for (int i = 0; i <= NU; ++i) {
                const Float3 rock = mix(rockA, rockB, static_cast<float>(i) / NU);
                for (int j = 0; j <= NV; ++j) {
                    // normal por diferencias de la propia malla
                    const Float3& l = pos[(std::max)(i - 1, 0)][j], &r = pos[(std::min)(i + 1, NU)][j];
                    const Float3& d = pos[i][(std::max)(j - 1, 0)], &t = pos[i][(std::min)(j + 1, NV)];
                    const DirectX::XMVECTOR du = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&r), DirectX::XMLoadFloat3(&l));
                    const DirectX::XMVECTOR dv = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&t), DirectX::XMLoadFloat3(&d));
                    Float3 nrm;
                    const DirectX::XMVECTOR cross = DirectX::XMVector3Cross(du, dv);
                    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(cross)) < 1.0e-8f) nrm = normal; // malla degenerada (pared sin altura)
                    else DirectX::XMStoreFloat3(&nrm, DirectX::XMVector3Normalize(cross));
                    if (nrm.x * normal.x + nrm.z * normal.z < 0.0f) nrm = { -nrm.x, -nrm.y, -nrm.z };
                    const float v = static_cast<float>(j) / NV;
                    const float k = grain(pos[i][j].x, pos[i][j].y, pos[i][j].z) * (0.55f + 0.20f * v) * (0.85f + 0.3f * (std::min)(bulge[i][j], 1.0f));
                    index[i][j] = vertex(pos[i][j], nrm, { rock.x * k, rock.y * k, rock.z * k });
                }
            }
            for (int i = 0; i < NU; ++i) for (int j = 0; j < NV; ++j) {
                triangle(index[i][j], index[i + 1][j], index[i][j + 1], normal);
                triangle(index[i + 1][j], index[i + 1][j + 1], index[i][j + 1], normal);
            }
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
                    if (m_batch.vertices.size() + 4 * 25 + 16 > MAX_VERTICES) flush();
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
                            wall(e.ca, e.ra, e.cb, e.rb, low, top, { -e.inward.x, 0.0f, -e.inward.z }, 0.0f);
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
