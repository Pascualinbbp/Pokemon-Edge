#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"

namespace PauseComponent {
    inline void render(GameState& state) {
        GuiLayout::dimBackground();

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.25f);
        GuiLayout::centeredText("PAUSA");

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        if (GuiLayout::centeredButton("CONTINUAR") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            state = GameState::PLAYING;
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("CONTROLES")) state = GameState::CONTROLS;

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (GuiLayout::centeredButton("MENU PRINCIPAL")) state = GameState::MAIN_MENU;
    }
}