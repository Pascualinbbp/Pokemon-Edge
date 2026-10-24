#pragma once
#include <algorithm>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace LoadingComponent {
    // Duración mínima de la pantalla de carga (la barra avanza por tiempo mientras el mundo se genera en otro hilo).
    inline constexpr float MIN_DURATION = 1.5f;

    // elapsed: segundos desde que empezó la carga; ready: el mundo ya está listo. Devuelve true el fotograma en que hay que entregarlo.
    inline bool render(GameState& state, float elapsed, bool ready) {
        const float progress = ready ? (std::min)(elapsed / MIN_DURATION, 1.0f) : (std::min)(elapsed / MIN_DURATION, 1.0f) * 0.95f;

        GuiLayout::beginAt(0.45f);
        GuiLayout::centeredText("CARGANDO MUNDO...");
        GuiLayout::gap(GuiLayout::GAP_MEDIUM);

        GuiLayout::centerX(GuiLayout::BUTTON_SIZE.x);
        ImGui::ProgressBar(progress, ImVec2(GuiLayout::BUTTON_SIZE.x, 18.0f));

        if (ready && elapsed >= MIN_DURATION) { state = GameState::PLAYING; return true; }
        return false;
    }
}
