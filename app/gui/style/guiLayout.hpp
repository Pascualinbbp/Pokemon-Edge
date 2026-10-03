#pragma once
#include "imgui.h"

// Piezas de maquetación compartidas por todas las pantallas.
namespace GuiLayout {
    inline const ImVec2 BUTTON_SIZE(340.0f, 48.0f);

    inline constexpr float GAP_SMALL = 10.0f;
    inline constexpr float GAP_MEDIUM = 15.0f;
    inline constexpr float GAP_LARGE = 20.0f;

    // Centra horizontalmente el siguiente elemento dentro de la ventana actual.
    inline void centerX(float itemWidth) {
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - itemWidth) * 0.5f);
    }

    inline void gap(float height) {
        ImGui::Dummy(ImVec2(0.0f, height));
    }

    // Coloca el cursor a una fracción de la altura de la ventana (inicio de cada pantalla).
    inline void beginAt(float heightFraction) {
        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * heightFraction);
    }

    inline void centeredText(const char* text) {
        centerX(ImGui::CalcTextSize(text).x);
        ImGui::TextUnformatted(text);
    }

    inline void centeredText(const char* text, ImU32 color) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        centeredText(text);
        ImGui::PopStyleColor();
    }

    inline bool centeredButton(const char* label) {
        centerX(BUTTON_SIZE.x);
        return ImGui::Button(label, BUTTON_SIZE);
    }

    // Botón centrado seguido de un espacio, para listas de botones.
    inline bool menuButton(const char* label, float gapAfter = GAP_MEDIUM) {
        const bool clicked = centeredButton(label);
        gap(gapAfter);
        return clicked;
    }

    // Velo oscuro sobre la escena congelada (menús dentro de la partida).
    inline void dimBackground() {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 150));
    }
}