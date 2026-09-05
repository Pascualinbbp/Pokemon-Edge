// C:\Users\bran\Desktop\Pokemon Edge\app\gui\guiStyle.hpp
#pragma once
#include "imgui.h"

namespace GuiStyle {
    inline void applyTheme() {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 5.0f;
        style.ScrollbarRounding = 3.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg]         = ImVec4(0.04f, 0.04f, 0.07f, 1.00f);
        colors[ImGuiCol_Header]           = ImVec4(0.003f, 0.725f, 0.992f, 0.80f);
        colors[ImGuiCol_Button]           = ImVec4(0.003f, 0.725f, 0.992f, 1.00f);
        colors[ImGuiCol_ButtonHovered]    = ImVec4(0.00f, 0.211f, 0.952f, 1.00f);
        colors[ImGuiCol_ButtonActive]     = ImVec4(0.002f, 0.15f, 0.70f, 1.00f);
        colors[ImGuiCol_FrameBg]          = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
        colors[ImGuiCol_Separator]        = ImVec4(0.003f, 0.725f, 0.992f, 0.50f);
        colors[ImGuiCol_CheckMark]        = ImVec4(0.003f, 0.725f, 0.992f, 1.00f);
        colors[ImGuiCol_SliderGrab]       = ImVec4(0.003f, 0.725f, 0.992f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.211f, 0.952f, 1.00f);
    }
}