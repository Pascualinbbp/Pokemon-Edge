#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace MenuComponent {
    enum class Action { NONE, NEW_GAME, CONTINUE_GAME };

    // "CONTINUAR PARTIDA" solo se muestra si hay una partida guardada.
    inline Action render(GameState& state, bool hasSave) {
        Action action = Action::NONE;

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.3f);
        if (hasSave) {
            if (GuiLayout::centeredButton("CONTINUAR PARTIDA")) action = Action::CONTINUE_GAME;
            ImGui::Dummy(ImVec2(0.0f, 15.0f));
        }

        if (GuiLayout::centeredButton("NUEVA PARTIDA")) action = Action::NEW_GAME;

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("VOLVER AL TÍTULO")) state = GameState::TITLE_SCREEN;

        if (action != Action::NONE) state = GameState::LOADING;
        return action;
    }
}