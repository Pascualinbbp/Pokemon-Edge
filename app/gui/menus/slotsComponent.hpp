#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../managers/saveManager.hpp"
#include "../../utils/core/stringUtil.hpp"

// Pantallas con la lista de partidas guardadas (cargar una, o elegir cuál eliminar).
namespace SlotsComponent {
    namespace detail {
        // Un botón por ranura. Devuelve la pulsada o -1.
        inline int slotButtons(bool onlyUsed) {
            int clicked = -1;
            const SaveManager::Slots& slots = SaveManager::slots();
            for (int slot = 0; slot < SaveManager::MAX_SLOTS; ++slot) {
                if (onlyUsed && !slots[slot].used) continue;

                char buffer[96];
                const char* label = StringUtil::formatTo(buffer, "PARTIDA %d   %s###slot%d",
                    slot + 1, slots[slot].savedAtText.c_str(), slot);
                if (GuiLayout::menuButton(label, GuiLayout::GAP_SMALL)) clicked = slot;
            }
            return clicked;
        }
    }

    // "CARGAR PARTIDA": devuelve la ranura elegida o -1.
    inline int renderLoad(GameState& state) {
        GuiLayout::beginAt(0.2f);
        GuiLayout::centeredText("CARGAR PARTIDA");
        GuiLayout::gap(GuiLayout::GAP_LARGE);

        const int slot = detail::slotButtons(true);

        if (GuiInput::backButton()) state = GameState::MAIN_MENU;
        return slot;
    }

    // Con todas las ranuras ocupadas: elegir una y confirmar su eliminación.
    // 'selected' es la ranura pendiente de confirmar (-1 si ninguna). Devuelve la ranura confirmada o -1.
    inline int renderReplace(GameState& state, int& selected) {
        int confirmed = -1;
        const bool back = GuiInput::backPressed();
        char buffer[96];

        GuiLayout::beginAt(0.15f);
        GuiLayout::centeredText(StringUtil::formatTo(buffer, "HAY %d PARTIDAS GUARDADAS", SaveManager::MAX_SLOTS));
        GuiLayout::centeredText("Elige una para eliminarla y empezar la nueva partida", GuiStyle::MUTED);
        GuiLayout::gap(GuiLayout::GAP_LARGE);

        if (selected < 0) {
            const int slot = detail::slotButtons(false);
            if (slot >= 0) selected = slot;

            if (GuiLayout::centeredButton("VOLVER") || back) state = GameState::MAIN_MENU;
        } else {
            GuiLayout::centeredText(StringUtil::formatTo(buffer, "¿Eliminar la partida %d? No se puede deshacer.", selected + 1),
                GuiStyle::DANGER);
            GuiLayout::gap(GuiLayout::GAP_MEDIUM);

            if (GuiLayout::menuButton("ELIMINAR Y EMPEZAR", GuiLayout::GAP_SMALL)) {
                confirmed = selected;
                selected = -1;
            }
            if (GuiLayout::centeredButton("CANCELAR") || back) selected = -1;
        }
        return confirmed;
    }
}
