#pragma once
#include <cstdio>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../managers/saveManager.hpp"

// Pantallas con la lista de partidas guardadas (cargar una, o elegir cuál eliminar).
namespace SlotsComponent {
    namespace detail {
        // Un botón por ranura. Devuelve la pulsada o -1.
        inline int slotButtons(bool onlyUsed) {
            int clicked = -1;
            const SaveManager::Slots& slots = SaveManager::slots();
            for (int slot = 0; slot < SaveManager::MAX_SLOTS; ++slot) {
                if (onlyUsed && !slots[slot].used) continue;

                char label[96];
                std::snprintf(label, sizeof(label), "PARTIDA %d   %s###slot%d", slot + 1, slots[slot].savedAtText.c_str(), slot);
                if (GuiLayout::centeredButton(label)) clicked = slot;
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
            }
            return clicked;
        }

        inline void mutedText(const char* text) {
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
            GuiLayout::centeredText(text);
            ImGui::PopStyleColor();
        }
    }

    // "CARGAR PARTIDA": devuelve la ranura elegida o -1.
    inline int renderLoad(GameState& state) {
        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.2f);
        GuiLayout::centeredText("CARGAR PARTIDA");
        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        const int slot = detail::slotButtons(true);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        if (GuiLayout::centeredButton("VOLVER") || GuiInput::backPressed()) state = GameState::MAIN_MENU;
        return slot;
    }

    // Con todas las ranuras ocupadas: elegir una y confirmar su eliminación.
    // 'selected' es la ranura pendiente de confirmar (-1 si ninguna). Devuelve la ranura confirmada o -1.
    inline int renderReplace(GameState& state, int& selected) {
        int confirmed = -1;
        const bool back = GuiInput::backPressed();

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.15f);
        GuiLayout::centeredText("HAY 4 PARTIDAS GUARDADAS");
        detail::mutedText("Elige una para eliminarla y empezar la nueva partida");
        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        if (selected < 0) {
            const int slot = detail::slotButtons(false);
            if (slot >= 0) selected = slot;

            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            if (GuiLayout::centeredButton("VOLVER") || back) state = GameState::MAIN_MENU;
        } else {
            char message[64];
            std::snprintf(message, sizeof(message), "¿Eliminar la partida %d? No se puede deshacer.", selected + 1);
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::DANGER);
            GuiLayout::centeredText(message);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 15.0f));
            if (GuiLayout::centeredButton("ELIMINAR Y EMPEZAR")) {
                confirmed = selected;
                selected = -1;
            }
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            if (GuiLayout::centeredButton("CANCELAR") || back) selected = -1;
        }
        return confirmed;
    }
}