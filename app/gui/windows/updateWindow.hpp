// C:\Users\bran\Desktop\Pokemon Edge\app\gui\windows\updateWindow.hpp
#pragma once
#include "imgui.h"
#include "appState.hpp"

namespace UpdateWindow {
    inline void render(GameState& gameState) {
        if (ImGui::Button("< Menú Principal")) {
            gameState = GameState::MAIN_MENU;
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.6f, 1.0f), "  |  MODULO: GESTOR DE ACTUALIZACIONES");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        ImGui::Text("Estado actual: El sistema está sincronizado con la última versión remota.");
        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        if (ImGui::Button("Buscar actualizaciones manualmente", ImVec2(300, 40))) {
            // Lógica de comprobación manual
        }
    }
}