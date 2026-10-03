#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace PauseComponent {
    inline void render(GameState& state) {
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const ImVec2 buttonSize(340.0f, 48.0f);

        // Velo oscuro sobre la escena congelada.
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), windowSize, IM_COL32(0, 0, 0, 150));

        const char* title = "PAUSA";
        ImGui::SetCursorPosY(windowSize.y * 0.3f);
        GuiLayout::centerX(ImGui::CalcTextSize(title).x);
        ImGui::TextUnformatted(title);

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        if (GuiLayout::centeredButton("CONTINUAR", buttonSize) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            state = GameState::PLAYING;
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("MENU PRINCIPAL", buttonSize)) {
            state = GameState::MAIN_MENU;
        }
    }
}