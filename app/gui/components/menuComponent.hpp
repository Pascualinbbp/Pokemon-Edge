#pragma once
#include "imgui.h"
#include "../mainWindow.hpp"

namespace MenuComponent {
    inline void render(GameState& state) {
        ImVec2 windowSize = ImGui::GetWindowSize();
        
        ImGui::SetCursorPosY(windowSize.y * 0.3f);
        float btnWidth = 340.0f;
        
        ImGui::SetCursorPosX((windowSize.x - btnWidth) * 0.5f);
        if (ImGui::Button("NUEVA PARTIDA", ImVec2(btnWidth, 48))) {
            // Placeholder
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        ImGui::SetCursorPosX((windowSize.x - btnWidth) * 0.5f);
        if (ImGui::Button("VOLVER AL TÍTULO", ImVec2(btnWidth, 48))) {
            state = GameState::TITLE_SCREEN;
        }
    }
}