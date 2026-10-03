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

        GuiLayout::beginAt(0.3f);
        if (hasSaves && GuiLayout::menuButton("CARGAR PARTIDA")) action = Action::LOAD_GAME;
        if (GuiLayout::menuButton("NUEVA PARTIDA")) action = Action::NEW_GAME;
        if (GuiLayout::menuButton("VOLVER AL TÍTULO")) state = GameState::TITLE_SCREEN;

        return action;
    }
}