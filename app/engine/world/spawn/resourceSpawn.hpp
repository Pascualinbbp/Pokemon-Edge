#pragma once
#include <algorithm>
#include <cmath>
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
    inline constexpr float MIN_LAND = 0.35f;        // altura mínima del terreno para colocar un nodo

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

        const auto free = [&](float x, float z) {
            if (world.groundHeight(x, z) < MIN_LAND) return false; // nunca en el agua
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

        for (int row = 0; row < chunks; ++row) {
            for (int col = 0; col < chunks; ++col) {
                const float x0 = -half + col * CHUNK, z0 = -half + row * CHUNK;
                habitats.weights(x0 + CHUNK * 0.5f, z0 + CHUNK * 0.5f, weights);
                float count = weights.empty() ? 3.0f : 0.0f;
                for (size_t h = 0; h < weights.size() && h < data.habitats.size(); ++h) {
                    count += weights[h] * between(static_cast<float>(data.habitats[h].minNodes), static_cast<float>(data.habitats[h].maxNodes) + 0.99f);
                }

                for (int i = 0; i < static_cast<int>(count); ++i) {
                    float x = 0.0f, z = 0.0f;
                    bool placed = false;
                    for (int attempt = 0; attempt < PLACE_TRIES && !placed; ++attempt) {
                        x = between(x0 + 1.0f, x0 + CHUNK - 1.0f);
                        z = between(z0 + 1.0f, z0 + CHUNK - 1.0f);
                        placed = free(x, z);
                    }
                    if (!placed) continue;

                    // Peso de cada tipo aquí: suma de lo que pide cada hábitat del punto, según lo que pesa.
                    habitats.weights(x, z, weights);
                    std::fill(typeWeights.begin(), typeWeights.end(), 0.0f);
                    for (size_t h = 0; h < weights.size() && h < data.habitats.size(); ++h) {
                        for (const ResourceChance& chance : data.habitats[h].resources) {
                            if (const int type = nodeIndex(chance.nodeId); type >= 0) typeWeights[type] += weights[h] * chance.weight;
                        }
                    }
                    float total = 0.0f;
                    for (const float w : typeWeights) total += w;
                    if (total <= 0.0f) continue;

                    float pick = between(0.0f, total);
                    int type = 0;
                    for (size_t t = 0; t < typeWeights.size(); ++t) {
                        pick -= typeWeights[t];
                        if (pick < 0.0f) { type = static_cast<int>(t); break; }
                    }
                    const ResourceNodeType& node = data.nodes[type];
                    const float growth = node.plant() ? between(PLANT_MIN_GROWTH, 1.0f) : 1.0f;
                    nodes.emplace_back(type, node.id, node.hits, node.plant(), static_cast<float>(node.plant() ? node.growSeconds : node.regrowSeconds),
                                       growth, DirectX::XMFLOAT3{ x, world.groundHeight(x, z), z }, between(0.0f, 6.2831853f));
                }
            }
        }
        return nodes;
    }
}
