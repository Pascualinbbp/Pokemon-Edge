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

        // Pokéball de HUD: mitad de color, mitad blanca, con banda y botón central.
        inline void drawBallIcon(ImDrawList* dl, const ImVec2& c, float r, const PokeballType& type, float alpha) {
            const int a = static_cast<int>(255.0f * alpha);
            const ImU32 color = IM_COL32(static_cast<int>(type.r * 255.0f), static_cast<int>(type.g * 255.0f), static_cast<int>(type.b * 255.0f), a);
            const ImU32 white = IM_COL32(240, 240, 240, a);
            const ImU32 dark = IM_COL32(20, 20, 25, a);

            dl->AddCircleFilled(c, r, white, 28);
            dl->PathArcTo(c, r, 3.1415927f, 6.2831853f, 20);
            dl->PathFillConvex(color);
            dl->AddCircle(c, r, dark, 28, 2.0f);
            dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), dark, 2.0f);
            dl->AddCircleFilled(c, r * 0.32f, white, 16);
            dl->AddCircle(c, r * 0.32f, dark, 16, 2.0f);
        }

        // Selector de pokéball (solo al apuntar): todas las del inventario, la equipada resaltada, con sus unidades
        // y el multiplicador de captura. Q / E o la rueda en teclado; L1 / R1 en mando.
        inline void drawBallSelector(ImDrawList* dl, const GameStatus& status, InputDevice device) {
            const Inventory& inventory = *status.inventory;
            const int count = inventory.size();
            const float alpha = status.aimBlend;
            const int a = static_cast<int>(255.0f * alpha);
            const ImVec2 screen = ImGui::GetIO().DisplaySize;

            constexpr float SPACING = 66.0f, SELECTED_RADIUS = 22.0f, OTHER_RADIUS = 15.0f;
            const float centerY = screen.y - 110.0f;
            const float startX = screen.x * 0.5f - SPACING * static_cast<float>(count - 1) * 0.5f;
            char buffer[96];

            for (int i = 0; i < count; ++i) {
                const bool selected = i == inventory.selectedIndex();
                const ImVec2 c(startX + SPACING * static_cast<float>(i), centerY);
                if (selected) dl->AddCircle(c, SELECTED_RADIUS + 6.0f, IM_COL32(255, 255, 255, a), 32, 2.5f);
                drawBallIcon(dl, c, selected ? SELECTED_RADIUS : OTHER_RADIUS, inventory.at(i).type, selected ? alpha : alpha * 0.55f);
                if (!selected) {
                    std::snprintf(buffer, sizeof(buffer), "x%d", inventory.at(i).count);
                    centered(dl, ImVec2(c.x, c.y + OTHER_RADIUS + 6.0f), buffer, 0.8f, IM_COL32(220, 220, 220, static_cast<int>(a * 0.7f)));
                }
            }

            const auto& slot = inventory.at(inventory.selectedIndex());
            std::snprintf(buffer, sizeof(buffer), "%s   x%d   Captura x%.1f", slot.type.name.c_str(), slot.count, slot.type.captureMultiplier);
            const ImU32 textColor = slot.count > 0 ? IM_COL32(255, 255, 255, a) : IM_COL32(235, 70, 60, a);
            centered(dl, ImVec2(screen.x * 0.5f, centerY + SELECTED_RADIUS + 26.0f), buffer, 1.0f, textColor);

            if (count > 1) {
                const bool pad = isGamepad(device);
                const float keyY = centerY - 14.0f;
                GuiDraw::keycap(dl, ImVec2(startX - SELECTED_RADIUS - 52.0f, keyY), pad ? "L1" : "Q", 28.0f, 28.0f);
                GuiDraw::keycap(dl, ImVec2(startX + SPACING * static_cast<float>(count - 1) + SELECTED_RADIUS + 24.0f, keyY), pad ? "R1" : "E", 28.0f, 28.0f);
            }
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

        if (status.aimBlend > 0.05f) {
            detail::drawCrosshair(dl, status);
            if (status.inventory && !status.inventory->empty()) detail::drawBallSelector(dl, status, device);
        }

        if (status.notice != 0) {
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const char* text = status.notice == 1 ? "¡CAPTURADO!" : status.notice == 2 ? "¡SE HA ESCAPADO!" : "¡SIN UNIDADES!";
            const ImU32 color = status.notice == 1 ? IM_COL32(80, 220, 110, 255) : status.notice == 2 ? IM_COL32(235, 70, 60, 255) : IM_COL32(255, 150, 40, 255);
            detail::centered(dl, ImVec2(size.x * 0.5f, size.y * 0.2f), text, 1.8f, color);
        }
    }
}
