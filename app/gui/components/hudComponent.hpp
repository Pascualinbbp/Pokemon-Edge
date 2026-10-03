#pragma once
#include "imgui.h"
#include "../gameState.hpp"

namespace HudComponent {
    inline void render(GameState& state) {
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "WASD: mover | RATON: camara | ESC: pausa | FPS: %.1f",
            ImGui::GetIO().Framerate);

        // repeat = false: mantener ESC pulsado no debe alternar el menú.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) state = GameState::PAUSED;
    }
}