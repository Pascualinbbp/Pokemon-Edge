#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace MenuComponent {
    inline void render(GameState& state) {
        const ImVec2 buttonSize(340.0f, 48.0f);

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.3f);
        if (GuiLayout::centeredButton("NUEVA PARTIDA", buttonSize)) {
            state = GameState::PLAYING;
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("VOLVER AL TÍTULO", buttonSize)) {
            state = GameState::TITLE_SCREEN;
        }
    }
}