#pragma once
#include <cstdio>
#include "imgui.h"
#include "../style/guiPrompts.hpp"
#include "../../engine/core/gameStatus.hpp"
#include "../../engine/core/inputDevice.hpp"
#include "../../engine/world/captureRules.hpp"

namespace HudComponent {
    namespace detail {
        // Color según la probabilidad de captura: verde muy alta, amarillo buena, naranja dudosa, rojo muy baja.
        inline ImU32 chanceColor(int percent, int alpha) {
            if (percent >= 70) return IM_COL32(80, 220, 110, alpha);
            if (percent >= 45) return IM_COL32(240, 220, 70, alpha);
            if (percent >= 25) return IM_COL32(255, 150, 40, alpha);
            return IM_COL32(235, 70, 60, alpha);
        }

        inline void centered(ImDrawList* dl, ImVec2 center, const char* text, float scale, ImU32 color) {
            ImFont* font = ImGui::GetFont();
            const float size = ImGui::GetFontSize() * scale;
            const ImVec2 extent = font->CalcTextSizeA(size, 1.0e9f, 0.0f, text);
            const ImVec2 pos(center.x - extent.x * 0.5f, center.y);
            dl->AddText(font, size, ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 200), text);
            dl->AddText(font, size, pos, color, text);
        }

        // Mira redonda (anillo con punto central); cambia de color al apuntar a un pokémon en rango.
        inline void drawCrosshair(ImDrawList* dl, const GameStatus& status) {
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const ImVec2 c(size.x * 0.5f, size.y * 0.5f);
            const int a = static_cast<int>(status.aimBlend * 255.0f);
            const ImU32 color = status.hasAimTarget ? chanceColor(status.chancePercent, a) : IM_COL32(255, 255, 255, a);
            const ImU32 shadow = IM_COL32(0, 0, 0, static_cast<int>(a * 0.6f));
            constexpr float RADIUS = 14.0f;

            dl->AddCircle(c, RADIUS, shadow, 0, 4.0f);
            dl->AddCircle(c, RADIUS, color, 0, 2.0f);
            dl->AddCircleFilled(c, 3.0f, shadow);
            dl->AddCircleFilled(c, 2.0f, color);

            if (status.hasAimTarget) {
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), "%d%%", status.chancePercent);
                centered(dl, ImVec2(c.x, c.y + RADIUS + 8.0f), buffer, 1.6f, color);

                float y = c.y + RADIUS + 8.0f + ImGui::GetFontSize() * 1.6f + 4.0f;
                if (status.behind) {
                    std::snprintf(buffer, sizeof(buffer), "ESPALDA x%.1f", CaptureRules::BACK_MULTIPLIER);
                    centered(dl, ImVec2(c.x, y), buffer, 0.9f, IM_COL32(120, 200, 255, a));
                    y += ImGui::GetFontSize() * 0.9f + 2.0f;
                }
                if (status.hidden) {
                    std::snprintf(buffer, sizeof(buffer), "SIGILO x%.0f", CaptureRules::STEALTH_MULTIPLIER);
                    centered(dl, ImVec2(c.x, y), buffer, 0.9f, IM_COL32(120, 200, 255, a));
                }
            }

            if (status.locked) centered(dl, ImVec2(c.x, c.y - RADIUS - 8.0f - ImGui::GetFontSize() * 0.9f), "FIJADO", 0.9f, IM_COL32(255, 255, 255, a));
        }
    }

    // La pausa con ESC la gestiona la escena (ESC sale primero del modo lanzamiento).
    inline void render(InputDevice device, const GameStatus& status) {
        ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
        ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

        // Ayudas de controles: el icono cambia solo según el dispositivo en uso.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        pos.y += 6.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::JUMP, "Saltar", device) + 24.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::CROUCH, "Agacharse", device) + 24.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::AIM, "Apuntar", device) + 24.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::THROW, "Lanzar", device) + 24.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::LOCK, "Fijar", device) + 24.0f;
        GuiPrompts::draw(dl, pos, GuiPrompts::Action::PAUSE, "Pausa", device);

        ImGui::Dummy(ImVec2(0.0f, 36.0f));
        ImGui::Text("Capturas: %d", status.captures);

        if (status.aimBlend > 0.05f) detail::drawCrosshair(dl, status);

        if (status.notice != 0) {
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const bool ok = status.notice == 1;
            detail::centered(dl, ImVec2(size.x * 0.5f, size.y * 0.2f), ok ? "¡CAPTURADO!" : "¡SE HA ESCAPADO!", 1.8f,
                ok ? IM_COL32(80, 220, 110, 255) : IM_COL32(235, 70, 60, 255));
        }
    }
}
