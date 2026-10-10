#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "imgui.h"
#include "mapTerrain.hpp"
#include "../style/guiPrompts.hpp"
#include "../style/icons/hudIcons.hpp"
#include "../../engine/core/gameStatus.hpp"

// Minimapa redondo arriba a la izquierda, centrado en el jugador (norte arriba): solo el terreno, las construcciones y una
// flecha hacia donde mira. En el borde: el clima (a la izquierda) y, arriba a la derecha, el disco de día y noche
// como una oreja: su mitad exterior asoma del círculo y el disco gira con el sol.
namespace MinimapComponent {
    namespace detail {
        inline constexpr float MARGIN = 14.0f;       // separación al borde izquierdo de la pantalla
        inline constexpr float TOP = 36.0f;          // separación arriba (deja sitio a los FPS)
        inline constexpr float RADIUS = 84.0f;       // mayor que los círculos del equipo
        inline constexpr float VIEW = 36.0f;         // metros que se ven desde el centro hasta el borde
        inline constexpr float BADGE = 20.0f;        // radio del logo del clima
        inline constexpr float EAR = 26.0f;          // radio del disco de día y noche
        inline constexpr float PI = 3.14159265f;
        inline constexpr float EAR_ANGLE = PI * 0.25f;
        inline constexpr int ARC_STEPS = 24;

        // Insignia redonda con su logo (o un círculo de reserva si no hay imagen).
        inline void badge(ImDrawList* dl, const ImVec2& c, float radius, ImTextureID texture) {
            dl->AddCircleFilled(c, radius + 3.0f, IM_COL32(14, 16, 26, 225), 32);
            if (texture) dl->AddImage(texture, ImVec2(c.x - radius, c.y - radius), ImVec2(c.x + radius, c.y + radius));
            else dl->AddCircleFilled(c, radius * 0.5f, IM_COL32(255, 255, 255, 120), 16);
            dl->AddCircle(c, radius + 3.0f, IM_COL32(255, 255, 255, 170), 32, 1.5f);
        }

        // Semicírculo exterior del disco de día y noche (una "oreja"): su base plana se apoya en el círculo del mapa, hacia
        // 'outward' (radianes desde arriba, en sentido horario). La imagen del disco gira con el sol: mediodía con el sol hacia
        // fuera, medianoche con la luna.
        inline void dayNightEar(ImDrawList* dl, const ImVec2& c, float radius, float outward, float sunAngle) {
            const float rotation = sunAngle - PI * 0.5f;
            const float so = std::sin(outward), co = std::cos(outward);
            const float sr = std::sin(rotation), cr = std::cos(rotation);
            const auto place = [&](float x, float y) { return ImVec2(c.x + x * co - y * so, c.y + x * so + y * co); };

            ImVec2 outline[ARC_STEPS + 2];
            ImVec2 local[ARC_STEPS + 2];
            local[0] = ImVec2(0.0f, 0.0f);
            for (int i = 0; i <= ARC_STEPS; ++i) {
                const float t = PI * static_cast<float>(i) / ARC_STEPS;
                local[i + 1] = ImVec2(std::cos(t) * radius, -std::sin(t) * radius);
            }
            for (int i = 0; i < ARC_STEPS + 2; ++i) outline[i] = place(local[i].x, local[i].y);

            // Borde oscuro algo mayor, la imagen encima y el filo claro.
            ImVec2 border[ARC_STEPS + 2];
            for (int i = 0; i < ARC_STEPS + 2; ++i) border[i] = place(local[i].x * 1.12f, local[i].y * 1.12f);
            dl->AddConvexPolyFilled(border, ARC_STEPS + 2, IM_COL32(14, 16, 26, 235));

            if (const ImTextureID texture = HudIcons::dayNight()) {
                dl->PushTextureID(texture);
                dl->PrimReserve(ARC_STEPS * 3, ARC_STEPS + 2);
                const ImDrawIdx first = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
                for (int i = 0; i < ARC_STEPS + 2; ++i) {
                    // Punto de la imagen que cae aquí: se deshace el giro del sol.
                    const float u = local[i].x * cr + local[i].y * sr, v = -local[i].x * sr + local[i].y * cr;
                    dl->PrimWriteVtx(outline[i], ImVec2(0.5f + u / (2.0f * radius), 0.5f + v / (2.0f * radius)), IM_COL32_WHITE);
                }
                for (int i = 0; i < ARC_STEPS; ++i) {
                    dl->PrimWriteIdx(first);
                    dl->PrimWriteIdx(static_cast<ImDrawIdx>(first + i + 1));
                    dl->PrimWriteIdx(static_cast<ImDrawIdx>(first + i + 2));
                }
                dl->PopTextureID();
            } else {
                dl->AddConvexPolyFilled(outline, ARC_STEPS + 2, IM_COL32(110, 190, 255, 255));
            }
            dl->AddPolyline(border, ARC_STEPS + 2, IM_COL32(255, 255, 255, 190), ImDrawFlags_Closed, 1.5f);

            // Marca de "ahora": triángulo en el punto más exterior.
            const ImVec2 tip = place(0.0f, -radius * 1.12f - 1.0f);
            dl->AddTriangleFilled(tip, place(-4.0f, -radius * 1.12f - 8.0f), place(4.0f, -radius * 1.12f - 8.0f), IM_COL32(255, 255, 255, 235));
        }
    }

    // Centro del minimapa en pantalla (otros elementos del HUD cuelgan de él).
    inline ImVec2 center() {
        return ImVec2(detail::RADIUS + detail::BADGE * 2.0f + detail::MARGIN, detail::TOP + detail::RADIUS + 4.0f);
    }
    inline float bottom() { return center().y + detail::RADIUS + 10.0f; }

    inline void draw(ImDrawList* dl, const GameStatus& status, InputDevice device) {
        using namespace detail;
        if (!status.habitatMap || !status.data) return;
        const MapTerrain::Data& terrain = MapTerrain::get(status);

        const ImVec2 mid = center();
        const float scale = RADIUS / VIEW;
        const float cell = Exploration::CELL * scale;
        const auto toScreen = [&](float x, float z) { return ImVec2(mid.x + (x - status.playerX) * scale, mid.y - (z - status.playerZ) * scale); };

        // Disco de día y noche: su base es una cuerda cuyos extremos tocan el círculo del mapa, así que parece parte de él.
        const float earReach = std::sqrt((RADIUS + 4.0f) * (RADIUS + 4.0f) - EAR * EAR);
        dayNightEar(dl, ImVec2(mid.x + std::sin(EAR_ANGLE) * earReach, mid.y - std::cos(EAR_ANGLE) * earReach), EAR, EAR_ANGLE, status.dayAngle);

        // Fondo y terreno dentro del círculo.
        dl->AddCircleFilled(mid, RADIUS + 4.0f, IM_COL32(14, 16, 26, 235), 64);
        const int firstCol = (std::max)(Exploration::cellOf(status.playerX - VIEW), 0), lastCol = (std::min)(Exploration::cellOf(status.playerX + VIEW), Exploration::SIZE - 1);
        const int firstRow = (std::max)(Exploration::cellOf(status.playerZ - VIEW), 0), lastRow = (std::min)(Exploration::cellOf(status.playerZ + VIEW), Exploration::SIZE - 1);
        for (int row = firstRow; row <= lastRow; ++row) {
            for (int col = firstCol; col <= lastCol; ++col) {
                const ImVec2 corner = toScreen(Exploration::centerOf(col) - Exploration::CELL * 0.5f, Exploration::centerOf(row) + Exploration::CELL * 0.5f);
                const float dx = corner.x + cell * 0.5f - mid.x, dy = corner.y + cell * 0.5f - mid.y;
                if (dx * dx + dy * dy > (RADIUS - 1.0f) * (RADIUS - 1.0f)) continue;
                dl->AddRectFilled(corner, ImVec2(corner.x + cell + 0.5f, corner.y + cell + 0.5f), terrain.colors[static_cast<size_t>(row) * Exploration::SIZE + col]);
            }
        }

        // Construcciones (paredes y máquina): las que caen dentro del círculo.
        for (const DirectX::XMFLOAT4& block : status.mapBlocks) {
            const ImVec2 lo = toScreen(block.x - block.z, block.y + block.w), hi = toScreen(block.x + block.z, block.y - block.w);
            const float cx = (lo.x + hi.x) * 0.5f - mid.x, cy = (lo.y + hi.y) * 0.5f - mid.y;
            if (cx * cx + cy * cy > (RADIUS - 4.0f) * (RADIUS - 4.0f)) continue;
            dl->AddRectFilled(lo, hi, IM_COL32(60, 52, 48, 255));
            dl->AddRect(lo, hi, IM_COL32(0, 0, 0, 200), 0.0f, 0, 1.0f);
        }

        // Marco, norte y flecha del jugador (apunta hacia donde mira la cámara).
        dl->AddCircle(mid, RADIUS + 1.0f, IM_COL32(14, 16, 26, 235), 64, 7.0f);
        dl->AddCircle(mid, RADIUS + 4.0f, IM_COL32(255, 255, 255, 200), 64, 1.8f);
        const ImVec2 northSize = ImGui::CalcTextSize("N");
        dl->AddText(ImVec2(mid.x - northSize.x * 0.5f, mid.y - RADIUS + 3.0f), IM_COL32(255, 255, 255, 220), "N");
        const float dirX = std::sin(status.playerYaw), dirY = -std::cos(status.playerYaw);
        const ImVec2 tip(mid.x + dirX * 9.0f, mid.y + dirY * 9.0f);
        const ImVec2 left(mid.x - dirY * 5.5f - dirX * 5.0f, mid.y + dirX * 5.5f - dirY * 5.0f);
        const ImVec2 right(mid.x + dirY * 5.5f - dirX * 5.0f, mid.y - dirX * 5.5f - dirY * 5.0f);
        dl->AddTriangleFilled(tip, left, right, IM_COL32(255, 255, 255, 255));
        dl->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 200), 1.5f);

        // Ayuda del mapa: a la derecha del círculo, a media altura.
        GuiPrompts::draw(dl, ImVec2(mid.x + RADIUS + 12.0f, mid.y - 12.0f), GuiPrompts::Action::MAP, "Mapa", device);

        // Clima a la izquierda, pegado al borde del círculo.
        badge(dl, ImVec2(mid.x - RADIUS - 4.0f - BADGE + 6.0f, mid.y), BADGE, HudIcons::weather(status.weatherName ? *status.weatherName : "Sol"));
    }
}
