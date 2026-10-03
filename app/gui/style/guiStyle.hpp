#pragma once
#include "imgui.h"

namespace GuiStyle {
    // Fondo de la GUI (también es el clear color fuera de la partida).
    inline constexpr float BACKGROUND[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    // Paleta compartida por los componentes que dibujan con ImDrawList.
    inline constexpr ImU32 ACCENT        = IM_COL32(0, 84, 250, 255); // azul de los botones
    inline constexpr ImU32 PANEL         = IM_COL32(18, 18, 24, 235);
    inline constexpr ImU32 SURFACE       = IM_COL32(32, 32, 42, 255);
    inline constexpr ImU32 FOREGROUND    = IM_COL32(235, 235, 240, 255);
    inline constexpr ImU32 MUTED         = IM_COL32(120, 120, 135, 255);
    inline constexpr ImU32 TAB_INACTIVE  = IM_COL32(40, 40, 54, 255);
    inline constexpr ImU32 SUCCESS       = IM_COL32(90, 220, 120, 255);

    inline void applyTheme() {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 5.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Button]        = ImVec4(0.00f, 0.33f, 0.98f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.10f, 0.43f, 1.00f, 1.00f);
        colors[ImGuiCol_ButtonActive]  = ImVec4(0.00f, 0.25f, 0.80f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImGui::ColorConvertU32ToFloat4(ACCENT); // barra de carga
    }
}