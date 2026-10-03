#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace MenuComponent {
    inline void render(GameState& state) {
        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.3f);
        if (GuiLayout::centeredButton("NUEVA PARTIDA")) state = GameState::PLAYING;

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("VOLVER AL TÍTULO")) state = GameState::TITLE_SCREEN;
    }
}