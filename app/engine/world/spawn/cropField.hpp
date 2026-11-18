#pragma once
#include <cmath>
#include <vector>
#include <DirectXMath.h>
#include "resourceSpawn.hpp"

// Parcelas de cultivo en la granja (mundo plano): tres zonas de cuatro parcelas. Cualquier parcela admite cualquier cultivo: el jugador elige qué semillas
// sembrar. Cada parcela es un ResourceNode vacío (no hay que arar: el suelo ya está preparado). El cultivo crece solo y se riega una vez por jugador y por
// pokémon (ver ResourceNode); recogido, la parcela vuelve a quedar libre para sembrar.
namespace Crops {
    struct Crop { int nodeId; const char* seedName; }; // nodo del cultivo (tabla resource_node) y material de sus semillas (tabla material); nullptr = se siembra el propio producto
    inline constexpr Crop LIST[] = { { 24, nullptr }, { 25, "Semillas de fresa" }, { 26, "Semillas de trigo" }, { 27, nullptr } };
    inline constexpr int COUNT = 4;
    inline constexpr int ZONES = 3;
    inline constexpr int PLOTS_PER_ZONE = 4;
    inline constexpr float ZONE_DISTANCE = 9.0f;  // del centro del mundo al centro de cada zona
    inline constexpr float PLOT_SPACING = 1.9f;   // entre parcelas (2 x 2)
    inline constexpr float PLOT_SIZE = 1.5f;      // lado de la tierra labrada
    inline constexpr float REACH = 1.9f;          // distancia a la que se puede sembrar

    // La granja: mundo plano aparte, lejos del archipiélago (fuera de la malla del terreno: no hay agua ni relieve, ni salen pokémon, cofres u objetos).
    // Las parcelas están solo aquí; el portal del centro devuelve al inicio.
    namespace Farm {
        inline constexpr float X = 3000.0f, Z = 3000.0f; // centro (m)
        inline constexpr float HALF = 28.0f;             // semilado del recinto cuadrado
    }

    struct Plot {
        DirectX::XMFLOAT3 position{};
        int node = -1;  // índice en Scene::nodes
    };

    // Etapas de crecimiento con modelo (assets/models/crops/<nodo>_<etapa>.glb); 0 = sin modelos (se dibuja con bloques). La fresa tendrá una tercera etapa.
    inline int stagesOf(int nodeId) {
        switch (nodeId) { case 24: return 3; case 25: return 3; case 27: return 3; default: return 0; }
    }

    inline int cropOf(int nodeId) {
        for (int i = 0; i < COUNT; ++i) if (LIST[i].nodeId == nodeId) return i;
        return -1;
    }

    // Índice en data.nodes del nodo de un cultivo (-1 si la base de datos no lo tiene).
    inline int typeIndexOf(const GameData& data, int crop) {
        for (size_t i = 0; i < data.nodes.size(); ++i) if (data.nodes[i].id == LIST[crop].nodeId) return static_cast<int>(i);
        return -1;
    }

    // Crea las parcelas (y sus nodos vacíos al final de 'nodes'). Las zonas se reparten alrededor del portal del centro de la granja.
    inline std::vector<Plot> generate(const GameData& data, const Physics::World& world, std::vector<ResourceNode>& nodes) {
        std::vector<Plot> plots;
        const float away = 1.0472f;
        for (int zone = 0; zone < ZONES; ++zone) {
            const int typeIndex = typeIndexOf(data, 0); // el tipo real lo fija la siembra (Plot vacío)
            if (typeIndex < 0) continue; // la base de datos aún no tiene los cultivos
            const float angle = away + zone * 2.0944f;
            const float cx = Farm::X + std::cos(angle) * ZONE_DISTANCE, cz = Farm::Z + std::sin(angle) * ZONE_DISTANCE;
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
