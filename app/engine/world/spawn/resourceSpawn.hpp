#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <vector>
#include <DirectXMath.h>
#include "../habitat/habitatMap.hpp"
#include "../entities/resourceNode.hpp"
#include "../state/researchMachine.hpp"
#include "../../../models/gameData.hpp"

// Materiales de recolección del mundo: se colocan una sola vez al generar el mundo (la misma semilla da el mismo mundo).
// Cada chunk de 16 m recibe tantos como pida su hábitat (min_nodes..max_nodes de la tabla habitat) y el tipo de cada uno
// se sortea según el peso que le dé cada hábitat del punto (tabla habitat_resource).
namespace ResourceSpawn {
    inline constexpr float CHUNK = 16.0f;
    inline constexpr float MIN_SEPARATION = 2.6f;   // distancia mínima entre dos nodos
    inline constexpr float CLEAR_RADIUS = 4.5f;     // zona libre alrededor del inicio, de la máquina y de las paredes
    inline constexpr float PLANT_MIN_GROWTH = 0.3f; // una planta de partida nueva tiene al menos este crecimiento
    inline constexpr int PLACE_TRIES = 12;
    inline constexpr float MAX_SLOPE = 0.55f;       // en laderas más empinadas no crece nada
    inline constexpr float DEEP_AREA_PER_NODE = 60.0f; // m² de caja envolvente de una galería por cada nodo
    inline constexpr float WALL_MIN = 0.35f;       // distancia a la pared de un nodo de galería: incrustado en ella, sin tapar el paso
    inline constexpr float WALL_MAX = 0.95f;
    inline constexpr float AQUATIC_MIN_DEPTH = 0.8f; // coral y algas: profundidad del agua en la que crecen
    inline constexpr float AQUATIC_MAX_DEPTH = 16.0f;
    inline constexpr float ENTRANCE_CLEAR = 7.0f;  // zona libre ante la bocana de cada cueva y mina

    // Altura a la que se asienta un nodo: la más baja del suelo bajo su base, para que no flote por el lado de cuesta abajo.
    inline float seat(const Physics::World& world, float x, float z, float half) {
        float low = world.groundHeight(x, z);
        for (int i = 0; i < 8; ++i) {
            const float a = i * 0.785398f;
            low = (std::min)(low, world.groundHeight(x + std::cos(a) * half, z + std::sin(a) * half));
        }
        return low - 0.04f;
    }

    inline std::vector<ResourceNode> generate(const GameData& data, const HabitatMap& habitats, unsigned seed,
                                              const Physics::World& world) {
        std::vector<ResourceNode> nodes;
        if (data.nodes.empty()) return nodes;

        std::mt19937 random(seed ^ 0x9E3779B9u);
        const auto between = [&](float low, float high) { return low + (high - low) * std::uniform_real_distribution<float>(0.0f, 1.0f)(random); };

        const float half = Physics::World::HALF_SIZE;
        const int chunks = static_cast<int>(2.0f * half / CHUNK);
        std::vector<float> weights, typeWeights(data.nodes.size());
        std::vector<int> typeIndexById;
        const auto nodeIndex = [&](int id) {
            for (size_t i = 0; i < data.nodes.size(); ++i) if (data.nodes[i].id == id) return static_cast<int>(i);
            return -1;
        };

        const auto fits = [&](float x, float z, bool aquatic) {
            const float depth = world.waterDepth(x, z);
            if (aquatic ? (depth < AQUATIC_MIN_DEPTH || depth > AQUATIC_MAX_DEPTH) : depth > 0.05f) return false; // solo coral y algas crecen en el agua
            if (world.groundHeight(x, z) < -50.0f || world.slope(x, z) > MAX_SLOPE) return false; // ni en una ladera empinada
            for (const Underground::System& system : habitats.under().systems()) {
                const float px = x - system.mouthX, pz = z - system.mouthZ;
                if (px * px + pz * pz < ENTRANCE_CLEAR * ENTRANCE_CLEAR) return false;
            }
            if (x * x + z * z < CLEAR_RADIUS * CLEAR_RADIUS) return false;
            const float mx = x - ResearchMachine::POSITION.x, mz = z - ResearchMachine::POSITION.z;
            if (mx * mx + mz * mz < CLEAR_RADIUS * CLEAR_RADIUS) return false;
            for (const Physics::World::Box& wall : world.obstacles) {
                if (std::fabs(x - wall.center.x) < wall.half.x + 2.0f && std::fabs(z - wall.center.z) < wall.half.z + 2.0f) return false;
            }
            for (const ResourceNode& other : nodes) {
                const float dx = x - other.body.position.x, dz = z - other.body.position.z;
                if (dx * dx + dz * dz < MIN_SEPARATION * MIN_SEPARATION) return false;
            }
            return true;
        };

        // Tipo de nodo que crece en (x, z) (en la galería de debajo si 'deep'): suma de lo que pide cada hábitat del punto, según lo que pesa.
        const auto pickType = [&](float x, float z, bool deep) -> std::optional<int> {
            habitats.weights(x, z, weights, deep);
            std::fill(typeWeights.begin(), typeWeights.end(), 0.0f);
            for (size_t h = 0; h < weights.size() && h < data.habitats.size(); ++h) {
                for (const ResourceChance& chance : data.habitats[h].resources) {
                    if (const int type = nodeIndex(chance.nodeId); type >= 0) typeWeights[type] += weights[h] * chance.weight;
                }
            }
            float total = 0.0f;
            for (const float w : typeWeights) total += w;
            if (total <= 0.0f) return std::nullopt;
            float pick = between(0.0f, total);
            int type = 0;
            for (size_t t = 0; t < typeWeights.size(); ++t) {
                pick -= typeWeights[t];
                if (pick < 0.0f) { type = static_cast<int>(t); break; }
            }
            return type;
        };
        const auto place = [&](int type, float x, float z, bool deep) {
            const ResourceNodeType& node = data.nodes[type];
            const float growth = node.plant() ? between(PLANT_MIN_GROWTH, 1.0f) : 1.0f;
            const float y = deep ? habitats.under().floorAt(x, z) - 0.04f : seat(world, x, z, ResourceStyle::look(node.id).half);
            nodes.emplace_back(type, node.id, node.hits, node.plant(), static_cast<float>(node.plant() ? node.growSeconds : node.regrowSeconds),
                               growth, DirectX::XMFLOAT3{ x, y, z }, between(0.0f, 6.2831853f));
            nodes.back().body.underground = deep;
        };

        for (int row = 0; row < chunks; ++row) {
            for (int col = 0; col < chunks; ++col) {
                const float x0 = -half + col * CHUNK, z0 = -half + row * CHUNK;
                if (!habitats.playable(x0 + CHUNK * 0.5f, z0 + CHUNK * 0.5f)) continue;
                habitats.weights(x0 + CHUNK * 0.5f, z0 + CHUNK * 0.5f, weights);
                float count = weights.empty() ? 3.0f : 0.0f;
                for (size_t h = 0; h < weights.size() && h < data.habitats.size(); ++h) {
                    count += weights[h] * between(static_cast<float>(data.habitats[h].minNodes), static_cast<float>(data.habitats[h].maxNodes) + 0.99f);
                }

                for (int i = 0; i < static_cast<int>(count); ++i) {
                    for (int attempt = 0; attempt < PLACE_TRIES; ++attempt) {
                        const float x = between(x0 + 1.0f, x0 + CHUNK - 1.0f), z = between(z0 + 1.0f, z0 + CHUNK - 1.0f);
                        const auto type = pickType(x, z, false);
                        if (!type || !fits(x, z, data.nodes[*type].aquatic)) continue;
                        place(*type, x, z, false);
                        break;
                    }
                }
            }
        }

        // Galerías: cada sistema recibe nodos según el hábitat de cada punto (mena en la mina, setas y cristales en la cueva...).
        for (const Underground::System& system : habitats.under().systems()) {
            const float area = (system.maxX - system.minX) * (system.maxZ - system.minZ);
            const int wanted = static_cast<int>(area / DEEP_AREA_PER_NODE);
            for (int i = 0, tries = 0; i < wanted && tries < wanted * 40; ++tries) {
                const float x = between(system.minX, system.maxX), z = between(system.minZ, system.maxZ);
                if (!habitats.under().covered(x, z) || habitats.under().depthAt(x, z) > 0.05f || habitats.under().floorAt(x, z) < -50.0f) continue;
                if (habitats.under().ceilAt(x, z) - habitats.under().floorAt(x, z) < 2.0f || habitats.under().floor().slope(x, z) > MAX_SLOPE) continue;
                const float wall = habitats.under().nearestWall(x, z, WALL_MAX + 1.0f);
                if (wall < WALL_MIN || wall > WALL_MAX) continue; // los minerales salen de las paredes, no del medio del paso
                const float mx = x - system.mouthX, mz = z - system.mouthZ;
                if (mx * mx + mz * mz < ENTRANCE_CLEAR * ENTRANCE_CLEAR) continue;
                bool apart = true;
                for (const ResourceNode& other : nodes) {
                    const float dx = x - other.body.position.x, dz = z - other.body.position.z;
                    if (dx * dx + dz * dz < MIN_SEPARATION * MIN_SEPARATION && other.body.underground) { apart = false; break; }
                }
                if (!apart) continue;
                if (const auto type = pickType(x, z, true)) { place(*type, x, z, true); ++i; }
            }
        }
        return nodes;
    }
}
