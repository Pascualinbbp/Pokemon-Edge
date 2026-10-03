#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"

namespace PauseComponent {
    // saved: mostrar el aviso de "partida guardada". Devuelve true si se pulsó "GUARDAR PARTIDA".
    inline bool render(GameState& state, bool saved) {
        GuiLayout::dimBackground();

        GuiLayout::beginAt(0.22f);
        GuiLayout::centeredText("PAUSA");
        GuiLayout::gap(GuiLayout::GAP_LARGE);

        // ESC, Options/Menú o el botón de volver del mando continúan la partida.
        if (GuiLayout::menuButton("CONTINUAR") || GuiInput::backPressed() || GuiInput::startPressed()) {
            state = GameState::PLAYING;
        }

        const bool saveRequested = GuiLayout::menuButton("GUARDAR PARTIDA");
        if (GuiLayout::menuButton("CONTROLES")) state = GameState::CONTROLS;
        if (GuiLayout::menuButton("MENU PRINCIPAL")) state = GameState::MAIN_MENU;

        if (saved) GuiLayout::centeredText("Partida guardada", GuiStyle::SUCCESS);
        return saveRequested;
    }
}