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
    };

    inline const Data& get(const GameStatus& status) {
        static Data data;
        const HabitatMap& map = *status.habitatMap;
        if (data.built && data.seed == map.seed()) return data;

        data.seed = map.seed();
        data.built = true;
        const size_t cells = static_cast<size_t>(Exploration::SIZE) * Exploration::SIZE;
        data.colors.assign(cells, IM_COL32(40, 60, 40, 255));
        data.habitat.assign(cells, -1);

        std::vector<DirectX::XMFLOAT3> colors;
        for (const Habitat& habitat : status.data->habitats) colors.push_back(HabitatStyle::color(habitat.name));
        std::vector<float> weights;
        for (int row = 0; row < Exploration::SIZE; ++row) {
            for (int col = 0; col < Exploration::SIZE; ++col) {
                map.weights(Exploration::centerOf(col), Exploration::centerOf(row), weights);
                if (weights.empty()) continue;
                float r = 0.0f, g = 0.0f, b = 0.0f, best = -1.0f;
                const size_t index = static_cast<size_t>(row) * Exploration::SIZE + col;
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
                const float shade = height > 0.0f ? 0.8f + 0.35f * (std::min)(height / 16.0f, 1.0f) : 1.0f; // la tierra alta se ve más clara
                data.colors[index] = IM_COL32((std::min)(255, int(r * shade * 255)), (std::min)(255, int(g * shade * 255)), (std::min)(255, int(b * shade * 255)), 255);
            }
        }
        return data;
    }
}
