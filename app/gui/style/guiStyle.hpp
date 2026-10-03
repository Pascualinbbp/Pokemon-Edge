#pragma once
#include "imgui.h"

namespace GuiStyle {
    // Color de fondo de la GUI: complementario del azul de los botones (0.00, 0.33, 0.98).
    inline constexpr float BACKGROUND[4] = { 0.98f, 0.70f, 0.00f, 1.00f };

    inline void applyTheme() {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 5.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg]      = ImVec4(BACKGROUND[0], BACKGROUND[1], BACKGROUND[2], BACKGROUND[3]);
        colors[ImGuiCol_Button]        = ImVec4(0.00f, 0.33f, 0.98f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.10f, 0.43f, 1.00f, 1.00f);
        colors[ImGuiCol_ButtonActive]  = ImVec4(0.00f, 0.25f, 0.80f, 1.00f);
    }
}