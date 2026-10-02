#pragma once
#include "imgui.h"
#include "../gameState.hpp"

namespace HudComponent {
    inline void render(GameState& state) {
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "Controles: WASD. FPS: %.1f", ImGui::GetIO().Framerate);
        if (ImGui::Button("SALIR AL MENU")) state = GameState::MAIN_MENU;
    }
}