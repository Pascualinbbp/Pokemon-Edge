#pragma once
#include "imgui.h"
#include "../mainWindow.hpp"

namespace TitleComponent {
    inline void render(GameState& state, void* logoTexture, int logoW, int logoH) {
        ImVec2 windowSize = ImGui::GetWindowSize();

        // Logo centrado
        ImGui::SetCursorPosY(windowSize.y * 0.2f);
        if (logoTexture) {
            float renderW = 320.0f;
            float renderH = (logoH > 0 && logoW > 0) ? (renderW * ((float)logoH / (float)logoW)) : 120.0f;
            ImGui::SetCursorPosX((windowSize.x - renderW) * 0.5f);
            ImGui::Image(logoTexture, ImVec2(renderW, renderH));
        }

        // Texto centrado (SIN BOTONES)
        ImGui::SetCursorPosY(windowSize.y * 0.75f);
        const char* pressText = "- PRESIONA CUALQUIER TECLA PARA CONTINUAR -";
        float pressWidth = ImGui::CalcTextSize(pressText).x;
        ImGui::SetCursorPosX((windowSize.x - pressWidth) * 0.5f);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "%s", pressText);

        // Transición de estado
        if (ImGui::IsAnyKeyPressed() || ImGui::IsMouseClicked(0)) {
            state = GameState::MAIN_MENU;
        }
    }
}