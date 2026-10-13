#pragma once
#include <algorithm>
#include <vector>
#include "imgui.h"
#include "../../engine/core/gameStatus.hpp"
#include "../../engine/world/state/exploration.hpp"
#include "../../engine/world/style/habitatStyle.hpp"

// Terreno del mundo para el minimapa y el mapa grande: color y hábitat dominante de cada celda de Exploration, calculados
// una sola vez por semilla.
namespace MapTerrain {
    struct Data {
        unsigned seed = 0;
        bool built = false;
        std::vector<ImU32> colors;
        std::vector<signed char> habitat; // hábitat dominante de la celda (-1 = ninguno)
        std::vector<unsigned char> valid; // 1 = la celda es mundo jugable (el archipiélago o una región); el resto no se dibuja
        std::vector<unsigned char> layer; // capa de la celda (HabitatMap::Layer)
    };

    inline constexpr const char* LAYER_NAMES[HabitatMap::LAYERS] = { "Subsuelo", "Superficie", "Cielo" };

    // ¿Se dibuja la celda en esa capa? La superficie incluye las montañas con cueva o mina encima; el subsuelo y el cielo solo lo suyo.
    inline bool shown(const Data& data, size_t index, int layer) {
        if (!data.valid[index]) return false;
        return layer == HabitatMap::SURFACE ? data.layer[index] != HabitatMap::SKY : data.layer[index] == layer;
    }

    inline const Data& get(const GameStatus& status) {
        static Data data;
        const HabitatMap& map = *status.habitatMap;
        if (data.built && data.seed == map.seed()) return data;

        data.seed = map.seed();
        data.built = true;
        const size_t cells = static_cast<size_t>(Exploration::SIZE) * Exploration::SIZE;
        data.colors.assign(cells, IM_COL32(40, 60, 40, 255));
        data.habitat.assign(cells, -1);
        data.valid.assign(cells, 0);
        data.layer.assign(cells, static_cast<unsigned char>(HabitatMap::SURFACE));

        std::vector<DirectX::XMFLOAT3> colors;
        for (const Habitat& habitat : status.data->habitats) colors.push_back(HabitatStyle::color(habitat.name));
        std::vector<float> weights;
        for (int row = 0; row < Exploration::SIZE; ++row) {
            for (int col = 0; col < Exploration::SIZE; ++col) {
                const size_t index = static_cast<size_t>(row) * Exploration::SIZE + col;
                if (!map.playable(Exploration::centerOf(col), Exploration::centerOf(row))) continue;
                data.valid[index] = 1;
                data.layer[index] = static_cast<unsigned char>(map.layerAt(Exploration::centerOf(col), Exploration::centerOf(row)));
                map.weights(Exploration::centerOf(col), Exploration::centerOf(row), weights);
                if (weights.empty()) continue;
                float r = 0.0f, g = 0.0f, b = 0.0f, best = -1.0f;
                for (size_t i = 0; i < weights.size() && i < colors.size(); ++i) {
                    r += weights[i] * colors[i].x;
                    g += weights[i] * colors[i].y;
                    b += weights[i] * colors[i].z;
                    if (weights[i] > best) {
                        best = weights[i];
                        data.habitat[index] = static_cast<signed char>(i);
                    }
                }
                const float height = map.height(Exploration::centerOf(col), Exploration::centerOf(row));
                float shade = height > 0.0f ? 0.8f + 0.35f * (std::min)(height / 16.0f, 1.0f) : 1.0f; // la tierra alta se ve más clara
                if (data.layer[index] == HabitatMap::UNDERGROUND) { // bajo tierra: el suelo claro y las paredes de roca oscuras
                    const HabitatMap::Region* region = map.regionAt(Exploration::centerOf(col), Exploration::centerOf(row));
                    shade = region && height - region->floorY < 4.0f ? 1.25f : 0.45f;
                } else if (data.layer[index] == HabitatMap::SKY) {
                    shade = 1.0f; // sin relieve de montaña ni nieve a esa altura
                }
                const float snow = data.layer[index] == HabitatMap::SURFACE ? (std::clamp)((height - HabitatMap::SNOW_LINE) / 6.0f, 0.0f, 1.0f) : 0.0f; // las cumbres nevadas se ven blancas
                const auto mix = [&](float c) { return (std::min)(255, int((c * shade * (1.0f - snow) + snow * 0.95f) * 255)); };
                data.colors[index] = IM_COL32(mix(r), mix(g), mix(b), 255);
            }
        }
        return data;
    }
}
