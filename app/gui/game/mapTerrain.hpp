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
        std::vector<unsigned char> layer; // capa de la celda en la superficie: SURFACE o SKY (HabitatMap::Layer)
        std::vector<ImU32> deepColors;    // lo mismo del subsuelo (galerías de cuevas y minas)
        std::vector<signed char> deepHabitat; // hábitat del subsuelo (-1 = no hay galería en la celda)
    };

    inline constexpr const char* LAYER_NAMES[HabitatMap::LAYERS] = { "Subsuelo", "Superficie", "Cielo" };

    // ¿Se dibuja la celda en esa capa? El subsuelo solo donde hay galería; la superficie y el cielo, lo suyo.
    inline bool shown(const Data& data, size_t index, int layer) {
        if (!data.valid[index]) return false;
        if (layer == HabitatMap::UNDERGROUND) return data.deepHabitat[index] >= 0;
        return layer == HabitatMap::SURFACE ? data.layer[index] != HabitatMap::SKY : data.layer[index] == layer;
    }
    inline ImU32 color(const Data& data, size_t index, int layer) { return layer == HabitatMap::UNDERGROUND ? data.deepColors[index] : data.colors[index]; }
    inline int habitat(const Data& data, size_t index, int layer) { return layer == HabitatMap::UNDERGROUND ? data.deepHabitat[index] : data.habitat[index]; }

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
        data.deepColors.assign(cells, IM_COL32(40, 40, 46, 255));
        data.deepHabitat.assign(cells, -1);

        std::vector<DirectX::XMFLOAT3> colors;
        for (const Habitat& habitat : status.data->habitats) colors.push_back(HabitatStyle::color(habitat.name));
        std::vector<float> weights;
        for (int row = 0; row < Exploration::SIZE; ++row) {
            for (int col = 0; col < Exploration::SIZE; ++col) {
                const size_t index = static_cast<size_t>(row) * Exploration::SIZE + col;
                if (!map.playable(Exploration::centerOf(col), Exploration::centerOf(row))) continue;
                data.valid[index] = 1;
                data.layer[index] = static_cast<unsigned char>(map.surfaceLayerAt(Exploration::centerOf(col), Exploration::centerOf(row)));
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
                if (data.layer[index] == HabitatMap::SKY) {
                    shade = 1.0f; // sin relieve de montaña ni nieve a esa altura
                }
                const float snow = data.layer[index] == HabitatMap::SURFACE ? (std::clamp)((height - HabitatMap::SNOW_LINE) / 6.0f, 0.0f, 1.0f) : 0.0f; // las cumbres nevadas se ven blancas
                const auto mix = [&](float c) { return (std::min)(255, int((c * shade * (1.0f - snow) + snow * 0.95f) * 255)); };
                data.colors[index] = IM_COL32(mix(r), mix(g), mix(b), 255);
            }
        }
        // Subsuelo: el hábitat de la galería en cada celda (la celda del mapa cuenta si alguno de sus puntos es galería cubierta).
        const Underground& under = map.under();
        for (int row = 0; row < Exploration::SIZE; ++row) {
            for (int col = 0; col < Exploration::SIZE; ++col) {
                const size_t index = static_cast<size_t>(row) * Exploration::SIZE + col;
                const float x = Exploration::centerOf(col), z = Exploration::centerOf(row);
                if (!data.valid[index] || !under.covered(x, z)) continue;
                const int h = under.habitatAt(x, z);
                if (h < 0 || h >= static_cast<int>(colors.size())) continue;
                data.deepHabitat[index] = static_cast<signed char>(h);
                const float shade = under.depthAt(x, z) > 0.35f ? 0.8f : 1.1f; // el agua subterránea se ve más oscura
                const auto mix = [&](float c) { return (std::min)(255, int(c * shade * 255)); };
                data.deepColors[index] = IM_COL32(mix(colors[h].x), mix(colors[h].y), mix(colors[h].z), 255);
            }
        }
        return data;
    }
}
