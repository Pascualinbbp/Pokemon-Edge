#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "imgui.h"
#include "../style/icons/hudIcons.hpp"
#include "../../engine/core/gameStatus.hpp"
#include "../../engine/world/style/habitatStyle.hpp"

// Minimapa redondo arriba a la izquierda, centrado en el jugador (norte arriba): suelo del color de cada hábitat, los
// pokémon salvajes cercanos como puntos y una flecha hacia donde mira. Alrededor: el clima (arriba a la izquierda), el
// disco de día y noche (arriba a la derecha, solo su mitad superior, que gira con el sol) y, algo separado a la izquierda,
// el hábitat donde está el jugador.
namespace MinimapComponent {
    namespace detail {
        inline constexpr float MARGIN = 26.0f;       // separación a los bordes de la pantalla
        inline constexpr float RADIUS = 84.0f;       // mayor que los círculos del equipo
        inline constexpr float VIEW = 36.0f;         // metros que se ven desde el centro hasta el borde
        inline constexpr float CELL = 2.0f;          // lado, en metros, de las celdas del mapa guardado
        inline constexpr float BADGE = 22.0f;        // radio de los logos de clima y de día/noche
        inline constexpr float HABITAT_BADGE = 27.0f;
        inline constexpr float GAP = 16.0f;          // separación entre el logo de hábitat y el mapa
        inline constexpr float PI = 3.14159265f;

        // Color de cada celda del mundo, calculado una vez por semilla.
        struct Cache {
            unsigned seed = 0;
            bool built = false;
            int size = 0;
            std::vector<ImU32> cells;
        };
        inline Cache& cache() {
            static Cache instance;
            return instance;
        }

        inline void build(const GameStatus& status) {
            Cache& c = cache();
            const HabitatMap& map = *status.habitatMap;
            c.seed = map.seed();
            c.built = true;
            c.size = static_cast<int>(2.0f * Physics::World::HALF_SIZE / CELL);
            c.cells.assign(static_cast<size_t>(c.size) * c.size, IM_COL32(40, 60, 40, 255));

            std::vector<DirectX::XMFLOAT3> colors;
            for (const Habitat& habitat : status.data->habitats) colors.push_back(HabitatStyle::color(habitat.name));
            std::vector<float> weights;
            for (int row = 0; row < c.size; ++row) {
                for (int col = 0; col < c.size; ++col) {
                    map.weights(-Physics::World::HALF_SIZE + (col + 0.5f) * CELL, -Physics::World::HALF_SIZE + (row + 0.5f) * CELL, weights);
                    float r = 0.0f, g = 0.0f, b = 0.0f;
                    for (size_t i = 0; i < weights.size() && i < colors.size(); ++i) {
                        r += weights[i] * colors[i].x;
                        g += weights[i] * colors[i].y;
                        b += weights[i] * colors[i].z;
                    }
                    if (!weights.empty()) c.cells[static_cast<size_t>(row) * c.size + col] = IM_COL32(int(r * 255), int(g * 255), int(b * 255), 255);
                }
            }
        }

        // Insignia redonda con su logo (o un círculo de reserva si no hay imagen).
        inline void badge(ImDrawList* dl, const ImVec2& c, float radius, ImTextureID texture) {
            dl->AddCircleFilled(c, radius + 3.0f, IM_COL32(14, 16, 26, 215), 32);
            if (texture) dl->AddImage(texture, ImVec2(c.x - radius, c.y - radius), ImVec2(c.x + radius, c.y + radius));
            else dl->AddCircleFilled(c, radius * 0.5f, IM_COL32(255, 255, 255, 120), 16);
            dl->AddCircle(c, radius + 3.0f, IM_COL32(255, 255, 255, 150), 32, 1.5f);
        }

        // Disco de día y noche: gira con el sol y solo se ve su mitad superior (mañana a la izquierda, tarde a la derecha).
        inline void dayNight(ImDrawList* dl, const ImVec2& c, float radius, float sunAngle) {
            const float rotation = sunAngle - PI * 0.5f; // sol arriba al mediodía; el sentido del giro es el de las agujas del reloj
            const float cs = std::cos(rotation), sn = std::sin(rotation);
            const auto corner = [&](float x, float y) { return ImVec2(c.x + x * cs - y * sn, c.y + x * sn + y * cs); };

            dl->PushClipRect(ImVec2(c.x - radius - 4.0f, c.y - radius - 4.0f), ImVec2(c.x + radius + 4.0f, c.y), true);
            dl->AddCircleFilled(c, radius + 3.0f, IM_COL32(14, 16, 26, 215), 32);
            if (const ImTextureID texture = HudIcons::dayNight()) {
                dl->AddImageQuad(texture, corner(-radius, -radius), corner(radius, -radius), corner(radius, radius), corner(-radius, radius));
            } else {
                dl->AddCircleFilled(c, radius * 0.9f, IM_COL32(110, 190, 255, 255), 32);
            }
            dl->AddCircle(c, radius + 3.0f, IM_COL32(255, 255, 255, 150), 32, 1.5f);
            dl->PopClipRect();
            dl->AddLine(ImVec2(c.x - radius - 3.0f, c.y), ImVec2(c.x + radius + 3.0f, c.y), IM_COL32(255, 255, 255, 170), 2.0f);
            dl->AddTriangleFilled(ImVec2(c.x, c.y - radius - 1.0f), ImVec2(c.x - 4.0f, c.y - radius - 8.0f), ImVec2(c.x + 4.0f, c.y - radius - 8.0f), IM_COL32(255, 255, 255, 230)); // marca de "ahora"
        }
    }

    // Centro del minimapa en pantalla (otros elementos del HUD cuelgan de él).
    inline ImVec2 center() {
        return ImVec2(detail::MARGIN + detail::HABITAT_BADGE * 2.0f + detail::GAP + detail::RADIUS + 6.0f, detail::MARGIN + detail::BADGE + 12.0f + detail::RADIUS);
    }
    inline float bottom() { return center().y + detail::RADIUS + 14.0f; }

    inline void draw(ImDrawList* dl, const GameStatus& status) {
        using namespace detail;
        if (!status.habitatMap || !status.data) return;
        Cache& c = cache();
        if (!c.built || c.seed != status.habitatMap->seed()) build(status);

        const ImVec2 mid = center();
        const float scale = RADIUS / VIEW;

        // Fondo y celdas del mundo dentro del círculo.
        dl->AddCircleFilled(mid, RADIUS + 4.0f, IM_COL32(14, 16, 26, 230), 64);
        const float half = Physics::World::HALF_SIZE;
        const int first = static_cast<int>(std::floor((-VIEW + status.playerX + half) / CELL));
        const int last = static_cast<int>(std::floor((VIEW + status.playerX + half) / CELL));
        const int firstRow = static_cast<int>(std::floor((-VIEW + status.playerZ + half) / CELL));
        const int lastRow = static_cast<int>(std::floor((VIEW + status.playerZ + half) / CELL));
        for (int row = (std::max)(firstRow, 0); row <= (std::min)(lastRow, c.size - 1); ++row) {
            for (int col = (std::max)(first, 0); col <= (std::min)(last, c.size - 1); ++col) {
                const float x0 = (-half + col * CELL - status.playerX) * scale, z0 = (-half + row * CELL - status.playerZ) * scale;
                const float cx = x0 + CELL * scale * 0.5f, cz = z0 + CELL * scale * 0.5f;
                if (cx * cx + cz * cz > (RADIUS - 1.0f) * (RADIUS - 1.0f)) continue;
                // Z hacia arriba: la celda se dibuja entre z0 y z0 + CELL
                dl->AddRectFilled(ImVec2(mid.x + x0, mid.y - z0 - CELL * scale), ImVec2(mid.x + x0 + CELL * scale + 0.5f, mid.y - z0 + 0.5f), c.cells[static_cast<size_t>(row) * c.size + col]);
            }
        }

        // Pokémon salvajes cercanos.
        for (const DirectX::XMFLOAT2& dot : status.wildDots) {
            const float dx = (dot.x - status.playerX) * scale, dz = (dot.y - status.playerZ) * scale;
            if (dx * dx + dz * dz > (RADIUS - 6.0f) * (RADIUS - 6.0f)) continue;
            dl->AddCircleFilled(ImVec2(mid.x + dx, mid.y - dz), 3.2f, IM_COL32(20, 20, 30, 230), 10);
            dl->AddCircleFilled(ImVec2(mid.x + dx, mid.y - dz), 2.2f, IM_COL32(255, 90, 80, 255), 10);
        }

        // Marco, norte y flecha del jugador (apunta hacia donde mira la cámara).
        dl->AddCircle(mid, RADIUS + 1.0f, IM_COL32(14, 16, 26, 235), 64, 7.0f);
        dl->AddCircle(mid, RADIUS + 4.0f, IM_COL32(255, 255, 255, 200), 64, 1.8f);
        const char* north = "N";
        const ImVec2 textSize = ImGui::CalcTextSize(north);
        dl->AddText(ImVec2(mid.x - textSize.x * 0.5f, mid.y - RADIUS + 3.0f), IM_COL32(255, 255, 255, 220), north);
        const float dirX = std::sin(status.playerYaw), dirY = -std::cos(status.playerYaw);
        const ImVec2 tip(mid.x + dirX * 9.0f, mid.y + dirY * 9.0f);
        const ImVec2 left(mid.x - dirY * 5.5f - dirX * 5.0f, mid.y + dirX * 5.5f - dirY * 5.0f);
        const ImVec2 right(mid.x + dirY * 5.5f - dirX * 5.0f, mid.y - dirX * 5.5f - dirY * 5.0f);
        dl->AddTriangleFilled(tip, left, right, IM_COL32(255, 255, 255, 255));
        dl->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 200), 1.5f);

        // Logos: clima arriba a la izquierda, día y noche arriba a la derecha y hábitat a la izquierda del mapa.
        const float diagonal = (RADIUS + 2.0f) * 0.7071f;
        badge(dl, ImVec2(mid.x - diagonal, mid.y - diagonal), BADGE, HudIcons::weather(status.weatherName ? *status.weatherName : "Sol"));
        dayNight(dl, ImVec2(mid.x + diagonal, mid.y - diagonal), BADGE, status.dayAngle);
        if (status.habitatName) badge(dl, ImVec2(mid.x - RADIUS - GAP - HABITAT_BADGE - 4.0f, mid.y), HABITAT_BADGE, HudIcons::habitat(*status.habitatName));
    }
}
