#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"

namespace PauseComponent {
    // La partida se guarda sola (periódicamente y al salir), así que no hay botón de guardar.
    inline void render(GameState& state) {
        GuiLayout::dimBackground();

        GuiLayout::beginAt(0.22f);
        GuiLayout::centeredText("PAUSA");
        GuiLayout::gap(GuiLayout::GAP_LARGE);

        // ESC, Options/Menú o el botón de volver del mando continúan la partida.
        if (GuiLayout::menuButton("CONTINUAR") || GuiInput::backPressed() || GuiInput::startPressed()) {
            state = GameState::PLAYING;
        }

        if (GuiLayout::menuButton("CONTROLES")) state = GameState::CONTROLS;
        if (GuiLayout::menuButton("MENU PRINCIPAL")) state = GameState::MAIN_MENU;
    }
}
