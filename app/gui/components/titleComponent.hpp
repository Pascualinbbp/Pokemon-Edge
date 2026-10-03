#pragma once
#include <cmath>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"
#include "../window/guiInput.hpp"

namespace TitleComponent {
    inline void render(GameState& state, ImTextureID logoTexture, int logoW, int logoH) {
        // Logo centrado
        GuiLayout::beginAt(0.2f);
        if (logoTexture) {
            const float renderW = 320.0f;
            const float renderH = (logoW > 0 && logoH > 0) ? renderW * (static_cast<float>(logoH) / logoW) : 120.0f;
            GuiLayout::centerX(renderW);
            ImGui::Image(logoTexture, ImVec2(renderW, renderH));
        }

        // Texto centrado con parpadeo suave
        GuiLayout::beginAt(0.75f);
        const char* pressText = "- PRESIONA CUALQUIER TECLA PARA CONTINUAR -";
        GuiLayout::centerX(ImGui::CalcTextSize(pressText).x);

        const float alpha = 0.2f + 0.8f * (0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f));
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, alpha), "%s", pressText);

        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
            ImGui::IsMouseClicked(0) || GuiInput::anyButtonPressed()) {
            state = GameState::MAIN_MENU;
        }
    }
}