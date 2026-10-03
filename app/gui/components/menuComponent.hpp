#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace MenuComponent {
    enum class Action { NONE, NEW_GAME, LOAD_GAME };

    // "CARGAR PARTIDA" solo se muestra si hay alguna partida guardada.
    // La decisión de qué hacer con cada acción la toma quien llama.
    inline Action render(GameState& state, bool hasSaves) {
        Action action = Action::NONE;

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.3f);
        if (hasSaves) {
            if (GuiLayout::centeredButton("CARGAR PARTIDA")) action = Action::LOAD_GAME;
            ImGui::Dummy(ImVec2(0.0f, 15.0f));
        }

        if (GuiLayout::centeredButton("NUEVA PARTIDA")) action = Action::NEW_GAME;

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("VOLVER AL TÍTULO")) state = GameState::TITLE_SCREEN;

        return action;
    }
}