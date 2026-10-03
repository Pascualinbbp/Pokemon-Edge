#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace ControlsComponent {
    namespace detail {
        struct Binding {
            const char* action;
            const char* key;
        };

        // Para añadir un control nuevo basta con añadir una fila aquí.
        inline constexpr Binding BINDINGS[] = {
            { "Mover adelante",   "W" },
            { "Mover atrás",      "S" },
            { "Mover izquierda",  "A" },
            { "Mover derecha",    "D" },
            { "Saltar",           "ESPACIO" },
            { "Mover la cámara",  "RATÓN" },
            { "Pausa / Volver",   "ESC" },
        };

        inline constexpr float TABLE_WIDTH = 460.0f;
        inline constexpr float KEY_COLUMN_WIDTH = 140.0f;
    }

    inline void render(GameState& state) {
        GuiLayout::dimBackground();

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.15f);
        GuiLayout::centeredText("CONTROLES");

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        GuiLayout::centerX(detail::TABLE_WIDTH);
        if (ImGui::BeginTable("controls", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH,
                ImVec2(detail::TABLE_WIDTH, 0.0f))) {
            ImGui::TableSetupColumn("ACCIÓN", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("TECLA", ImGuiTableColumnFlags_WidthFixed, detail::KEY_COLUMN_WIDTH);
            ImGui::TableHeadersRow();

            for (const detail::Binding& b : detail::BINDINGS) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(b.action);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(b.key);
            }
            ImGui::EndTable();
        }

        ImGui::Dummy(ImVec2(0.0f, 25.0f));
        if (GuiLayout::centeredButton("VOLVER") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            state = GameState::PAUSED;
        }
    }
}