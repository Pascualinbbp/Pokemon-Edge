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

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.22f);
        GuiLayout::centeredText("PAUSA");

        // ESC, Options/Menú o el botón de volver del mando continúan la partida.
        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        if (GuiLayout::centeredButton("CONTINUAR") || GuiInput::backPressed() || GuiInput::startPressed()) {
            state = GameState::PLAYING;
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        const bool saveRequested = GuiLayout::centeredButton("GUARDAR PARTIDA");

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("CONTROLES")) state = GameState::CONTROLS;

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("MENU PRINCIPAL")) state = GameState::MAIN_MENU;

        if (saved) {
            ImGui::Dummy(ImVec2(0.0f, 15.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::SUCCESS);
            GuiLayout::centeredText("Partida guardada");
            ImGui::PopStyleColor();
        }
        return saveRequested;
    }
}