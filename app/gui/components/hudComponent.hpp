#pragma once
#include <cmath>
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

        // Aviso de autoguardado (esquina inferior derecha): panel translúcido con el contorno de una pokéball
        // que gira y el texto "Guardando...". alpha (0..1) controla el fundido de entrada y salida.
        inline void drawSaving(ImDrawList* dl, float alpha) {
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            const auto a = [alpha](int value) { return static_cast<int>(value * alpha); };

            constexpr float RADIUS = 15.0f;
            constexpr float BUTTON = 5.5f;
            constexpr float MARGIN = 24.0f;
            constexpr float PAD_X = 16.0f;
            constexpr float PAD_Y = 11.0f;
            constexpr float GAP = 12.0f;
            constexpr float TEXT_SCALE = 1.1f;

            ImFont* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize() * TEXT_SCALE;
            const char* text = "Guardando...";
            const ImVec2 textSize = font->CalcTextSizeA(fontSize, 1.0e9f, 0.0f, text);

            const float panelW = PAD_X * 2.0f + RADIUS * 2.0f + GAP + textSize.x;
            const float panelH = PAD_Y * 2.0f + RADIUS * 2.0f;
            const ImVec2 p1(screen.x - MARGIN, screen.y - MARGIN);
            const ImVec2 p0(p1.x - panelW, p1.y - panelH);

            dl->AddRectFilled(p0, p1, IM_COL32(12, 12, 18, a(190)), 12.0f);
            dl->AddRect(p0, p1, IM_COL32(255, 255, 255, a(70)), 12.0f, 0, 1.5f);

            const ImVec2 c(p0.x + PAD_X + RADIUS, p0.y + panelH * 0.5f);
            const ImU32 color = IM_COL32(245, 245, 245, a(255));
            const ImU32 shadow = IM_COL32(0, 0, 0, a(160));

            const ImVec2 textPos(c.x + RADIUS + GAP, c.y - textSize.y * 0.5f);
            dl->AddText(font, fontSize, ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), shadow, text);
            dl->AddText(font, fontSize, textPos, color, text);

            // Solo bordes: aro exterior, ecuador giratorio con hueco para el botón central, y el aro del botón.
            const float angle = static_cast<float>(ImGui::GetTime()) * 5.0f;
            const ImVec2 d(std::cos(angle), std::sin(angle));
            dl->AddCircle(c, RADIUS, shadow, 32, 4.5f);
            dl->AddCircle(c, RADIUS, color, 32, 2.5f);
            dl->AddLine(ImVec2(c.x - d.x * RADIUS, c.y - d.y * RADIUS), ImVec2(c.x - d.x * BUTTON, c.y - d.y * BUTTON), color, 2.5f);
            dl->AddLine(ImVec2(c.x + d.x * BUTTON, c.y + d.y * BUTTON), ImVec2(c.x + d.x * RADIUS, c.y + d.y * RADIUS), color, 2.5f);
            dl->AddCircle(c, BUTTON, color, 16, 2.5f);
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
                    std::snprintf(buffer, sizeof(buffer), "ESPALDA x%.2f", CaptureRules::BACK_MULTIPLIER);
                    centered(dl, ImVec2(c.x, y), buffer, 0.9f, IM_COL32(120, 200, 255, a));
                    y += ImGui::GetFontSize() * 0.9f + 2.0f;
                }
                if (status.hidden) {
                    std::snprintf(buffer, sizeof(buffer), "SIGILO x%.2f", CaptureRules::STEALTH_MULTIPLIER);
                    centered(dl, ImVec2(c.x, y), buffer, 0.9f, IM_COL32(120, 200, 255, a));
                }
            }

            if (status.locked) centered(dl, ImVec2(c.x, c.y - RADIUS - 8.0f - ImGui::GetFontSize() * 0.9f), "FIJADO", 0.9f, IM_COL32(255, 255, 255, a));
        }
    }

    // La pausa con ESC la gestiona la escena (ESC sale primero del modo lanzamiento).
    // saving: opacidad del aviso de autoguardado (0 = oculto, 1 = totalmente visible).
    inline void render(InputDevice device, const GameStatus& status, float saving) {
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

        if (saving > 0.0f) detail::drawSaving(dl, saving);

        if (status.aimBlend > 0.05f) detail::drawCrosshair(dl, status);

        if (status.notice != 0) {
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const bool ok = status.notice == 1;
            detail::centered(dl, ImVec2(size.x * 0.5f, size.y * 0.2f), ok ? "¡CAPTURADO!" : "¡SE HA ESCAPADO!", 1.8f,
                ok ? IM_COL32(80, 220, 110, 255) : IM_COL32(235, 70, 60, 255));
        }
    }
}
