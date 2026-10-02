#pragma once
#include "imgui.h"

namespace GuiLayout {
    // Centra horizontalmente el siguiente elemento dentro de la ventana actual.
    inline void centerX(float itemWidth) {
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - itemWidth) * 0.5f);
    }

    inline bool centeredButton(const char* label, const ImVec2& size) {
        centerX(size.x);
        return ImGui::Button(label, size);
    }
}