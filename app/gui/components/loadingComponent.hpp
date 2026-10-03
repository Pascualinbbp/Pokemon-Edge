#pragma once
#include <algorithm>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace LoadingComponent {
    // Duración de la carga simulada. Más adelante será el progreso real (recursos + generación del mundo).
    inline constexpr float FAKE_DURATION = 1.5f;

    // elapsed: segundos desde que empezó la carga.
    inline void render(GameState& state, float elapsed) {
        const float progress = (std::min)(elapsed / FAKE_DURATION, 1.0f);

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.45f);
        GuiLayout::centeredText("CARGANDO MUNDO...");

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        GuiLayout::centerX(GuiLayout::BUTTON_SIZE.x);
        ImGui::ProgressBar(progress, ImVec2(GuiLayout::BUTTON_SIZE.x, 18.0f));

        if (progress >= 1.0f) state = GameState::PLAYING;
    }
}