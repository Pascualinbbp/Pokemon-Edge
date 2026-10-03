#pragma once
#include "imgui.h"

namespace GuiLayout {
    inline const ImVec2 BUTTON_SIZE(340.0f, 48.0f);

    // Centra horizontalmente el siguiente elemento dentro de la ventana actual.
    inline void centerX(float itemWidth) {
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - itemWidth) * 0.5f);
    }

    inline void centeredText(const char* text) {
        centerX(ImGui::CalcTextSize(text).x);
        ImGui::TextUnformatted(text);
    }

    inline bool centeredButton(const char* label) {
        centerX(BUTTON_SIZE.x);
        return ImGui::Button(label, BUTTON_SIZE);
    }

    // Velo oscuro sobre la escena congelada (menús dentro de la partida).
    inline void dimBackground() {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 150));
    }
}