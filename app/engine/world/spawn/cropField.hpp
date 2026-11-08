#pragma once
#include <cmath>
#include <vector>
#include <DirectXMath.h>
#include "resourceSpawn.hpp"
#include "../state/researchMachine.hpp"

// Parcelas de cultivo junto al punto de partida: tres zonas de cuatro parcelas. Cualquier parcela admite cualquier cultivo: el jugador elige qué semillas
// sembrar. Cada parcela es un ResourceNode vacío (no hay que arar: el suelo ya está preparado). El cultivo crece solo y se riega una vez por jugador y por
// pokémon (ver ResourceNode); recogido, la parcela vuelve a quedar libre para sembrar.
namespace Crops {
    struct Crop { int nodeId; const char* seedName; }; // nodo del cultivo (tabla resource_node) y material de sus semillas (tabla material)
    inline constexpr Crop LIST[] = { { 24, "Semillas de zanahoria" }, { 25, "Semillas de fresa" }, { 26, "Semillas de trigo" } };
    inline constexpr int COUNT = 3;
    inline constexpr int PLOTS_PER_ZONE = 4;
    inline constexpr float ZONE_DISTANCE = 9.0f;  // del centro del mundo al centro de cada zona
    inline constexpr float PLOT_SPACING = 1.9f;   // entre parcelas (2 x 2)
    inline constexpr float PLOT_SIZE = 1.5f;      // lado de la tierra labrada
    inline constexpr float REACH = 1.9f;          // distancia a la que se puede sembrar

    struct Plot {
        DirectX::XMFLOAT3 position{};
        int node = -1;  // índice en Scene::nodes
    };

    inline int cropOf(int nodeId) {
        for (int i = 0; i < COUNT; ++i) if (LIST[i].nodeId == nodeId) return i;
        return -1;
    }

    // Índice en data.nodes del nodo de un cultivo (-1 si la base de datos no lo tiene).
    inline int typeIndexOf(const GameData& data, int crop) {
        for (size_t i = 0; i < data.nodes.size(); ++i) if (data.nodes[i].id == LIST[crop].nodeId) return static_cast<int>(i);
        return -1;
    }

    // Crea las parcelas (y sus nodos vacíos al final de 'nodes'). Las zonas se reparten alrededor del inicio, lejos de la máquina de investigación.
    inline std::vector<Plot> generate(const GameData& data, const Physics::World& world, std::vector<ResourceNode>& nodes) {
        std::vector<Plot> plots;
        const float away = std::atan2(ResearchMachine::POSITION.z, ResearchMachine::POSITION.x) + 1.0472f;
        for (int zone = 0; zone < COUNT; ++zone) {
            const int typeIndex = typeIndexOf(data, 0); // el tipo real lo fija la siembra (Plot vacío)
            if (typeIndex < 0) continue; // la base de datos aún no tiene los cultivos
            const float angle = away + zone * 2.0944f;
            const float cx = std::cos(angle) * ZONE_DISTANCE, cz = std::sin(angle) * ZONE_DISTANCE;
            for (int k = 0; k < PLOTS_PER_ZONE; ++k) {
                const float x = cx + ((k % 2) - 0.5f) * PLOT_SPACING, z = cz + ((k / 2) - 0.5f) * PLOT_SPACING;
                const ResourceNodeType& type = data.nodes[typeIndex];
                nodes.emplace_back(typeIndex, type.id, type.hits, true, static_cast<float>(type.growSeconds), 0.0f,
                                   DirectX::XMFLOAT3{ x, ResourceSpawn::seat(world, x, z, PLOT_SIZE * 0.5f), z }, 0.0f, true, true);
                nodes.back().makePlot();
                plots.push_back({ { x, nodes.back().body.position.y, z }, static_cast<int>(nodes.size()) - 1 });
            }
        }
        return plots;
    }
}
