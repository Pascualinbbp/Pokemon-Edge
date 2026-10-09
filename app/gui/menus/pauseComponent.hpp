#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../window/guiInput.hpp"

// Menú de pausa: panel lateral a la derecha con todas las opciones (mochila, pokémon y PC, controles...).
namespace PauseComponent {
    namespace detail {
        inline constexpr float MARGIN = 36.0f;
        inline constexpr float PANEL_WIDTH = 400.0f;
        inline constexpr float ROW_HEIGHT = 84.0f;
        inline constexpr float ROW_GAP = 12.0f;

        enum class Icon { BAG, POKEBALL, MAP, CONTROLS, EXIT };

        inline void drawIcon(ImDrawList* dl, const ImVec2& c, float r, Icon icon) {
            const ImU32 white = IM_COL32(255, 255, 255, 255);
            switch (icon) {
                case Icon::BAG:
                    dl->AddRectFilled(ImVec2(c.x - r * 0.7f, c.y - r * 0.4f), ImVec2(c.x + r * 0.7f, c.y + r * 0.8f), white, r * 0.25f);
                    dl->AddCircle(ImVec2(c.x, c.y - r * 0.4f), r * 0.4f, white, 20, 2.5f);
                    break;
                case Icon::POKEBALL:
                    ItemIcon::ball(dl, c, r * 0.9f, { 0.90f, 0.20f, 0.20f });
                    break;
                case Icon::MAP:
                    dl->AddRectFilled(ImVec2(c.x - r * 0.8f, c.y - r * 0.7f), ImVec2(c.x + r * 0.8f, c.y + r * 0.7f), white, r * 0.15f);
                    dl->AddLine(ImVec2(c.x - r * 0.27f, c.y - r * 0.7f), ImVec2(c.x - r * 0.27f, c.y + r * 0.7f), IM_COL32(40, 40, 55, 255), 2.0f);
                    dl->AddLine(ImVec2(c.x + r * 0.27f, c.y - r * 0.7f), ImVec2(c.x + r * 0.27f, c.y + r * 0.7f), IM_COL32(40, 40, 55, 255), 2.0f);
                    dl->AddCircleFilled(ImVec2(c.x + r * 0.5f, c.y + r * 0.1f), r * 0.14f, IM_COL32(230, 70, 60, 255));
                    break;
                case Icon::CONTROLS:
                    dl->AddRectFilled(ImVec2(c.x - r * 0.9f, c.y - r * 0.5f), ImVec2(c.x + r * 0.9f, c.y + r * 0.5f), white, r * 0.4f);
                    dl->AddCircleFilled(ImVec2(c.x - r * 0.45f, c.y), r * 0.18f, IM_COL32(40, 40, 55, 255));
                    dl->AddCircleFilled(ImVec2(c.x + r * 0.35f, c.y - r * 0.12f), r * 0.12f, IM_COL32(40, 40, 55, 255));
                    dl->AddCircleFilled(ImVec2(c.x + r * 0.55f, c.y + r * 0.15f), r * 0.12f, IM_COL32(40, 40, 55, 255));
                    break;
                case Icon::EXIT:
                    dl->PathArcTo(c, r * 0.7f, -1.0f, 4.14f, 24);
                    dl->PathStroke(white, 0, 3.0f);
                    dl->AddLine(ImVec2(c.x, c.y - r * 0.85f), ImVec2(c.x, c.y), white, 3.0f);
                    break;
            }
        }

        // Una opción del panel. Devuelve true al elegirla.
        inline bool option(int id, Icon icon, const char* title, const char* subtitle, float width) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 a = ImGui::GetCursorScreenPos();
            const ImVec2 b(a.x + width, a.y + ROW_HEIGHT);
            ImGui::PushID(id);
            const bool clicked = ImGui::InvisibleButton("##option", ImVec2(width, ROW_HEIGHT));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            dl->AddRectFilled(a, b, hovered ? IM_COL32(70, 90, 160, 235) : IM_COL32(44, 46, 66, 225), 16.0f);
            dl->AddRect(a, b, hovered ? GuiCards::SELECT : GuiCards::EDGE, 16.0f, 0, hovered ? 2.5f : 1.5f);
            const ImVec2 c(a.x + 46.0f, a.y + ROW_HEIGHT * 0.5f);
            dl->AddCircleFilled(c, 28.0f, GuiStyle::ACCENT, 32);
            drawIcon(dl, c, 17.0f, icon);
            GuiCards::text(dl, ImVec2(a.x + 94.0f, a.y + 17.0f), IM_COL32(255, 255, 255, 255), title, 1.2f);
            GuiCards::text(dl, ImVec2(a.x + 94.0f, a.y + 46.0f), GuiStyle::MUTED, subtitle, 0.85f);
            ImGui::Dummy(ImVec2(0.0f, ROW_GAP));
            return clicked;
        }
    }

    // La partida se guarda sola (periódicamente y al salir), así que no hay botón de guardar.
    // ESC, Options/Menú o el botón de volver del mando continúan la partida.
    inline void render(GameState& state) {
        using namespace detail;
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 120));

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 a(screen.x - MARGIN - PANEL_WIDTH, MARGIN);
        const ImVec2 b(screen.x - MARGIN, screen.y - MARGIN);
        GuiCards::panel(dl, a, b);

        GuiCards::text(dl, ImVec2(a.x + 24.0f, a.y + 20.0f), IM_COL32(255, 255, 255, 255), "MENÚ", 1.5f);
        GuiCards::text(dl, ImVec2(a.x + 24.0f, b.y - 34.0f), GuiStyle::MUTED, "ESC: continuar", 0.9f);

        ImGui::SetCursorScreenPos(ImVec2(a.x + 20.0f, a.y + 74.0f));
        const float width = PANEL_WIDTH - 40.0f;
        if (option(0, Icon::BAG, "Mochila", "Objetos, materiales y herramientas", width)) state = GameState::INVENTORY;
        ImGui::SetCursorScreenPos(ImVec2(a.x + 20.0f, ImGui::GetCursorScreenPos().y));
        if (option(1, Icon::POKEBALL, "Pokémon", "Equipo y PC", width)) state = GameState::POKEMON;
        ImGui::SetCursorScreenPos(ImVec2(a.x + 20.0f, ImGui::GetCursorScreenPos().y));
        if (option(2, Icon::MAP, "Mapa", "Zonas exploradas y sus hábitats", width)) state = GameState::MAP;
        ImGui::SetCursorScreenPos(ImVec2(a.x + 20.0f, ImGui::GetCursorScreenPos().y));
        if (option(3, Icon::CONTROLS, "Controles", "Teclado y mando", width)) state = GameState::CONTROLS;
        ImGui::SetCursorScreenPos(ImVec2(a.x + 20.0f, ImGui::GetCursorScreenPos().y));
        if (option(4, Icon::EXIT, "Menú principal", "Guarda la partida y sale", width)) state = GameState::MAIN_MENU;

        if (GuiInput::backPressed() || GuiInput::startPressed()) state = GameState::PLAYING;
    }
}
