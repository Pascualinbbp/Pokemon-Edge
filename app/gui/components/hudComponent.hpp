#pragma once
#include "imgui.h"
#include "../gameState.hpp"

namespace HudComponent {
    inline void render(GameState& state) {
        ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
        ImGui::Text("ESC: pausa | FPS: %.0f", ImGui::GetIO().Framerate);

        // repeat = false: mantener ESC pulsado no debe alternar el menú.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) state = GameState::PAUSED;
    }
}