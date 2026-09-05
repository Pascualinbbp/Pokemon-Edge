// C:\Users\bran\Desktop\Pokemon Edge\app\gui\windows\mainWindow.hpp
#pragma once
#include "imgui.h"
#include "appState.hpp"
#include <string>

namespace MainWindow {
    inline void render(GameState& gameState) {
        ImVec2 windowSize = ImGui::GetWindowSize();
        
        ImGui::SetCursorPosY(windowSize.y * 0.2f);
        std::string menuTitle = "MENÚ PRINCIPAL // GESTOR";
        float mTitleWidth = ImGui::CalcTextSize(menuTitle.c_str()).x;
        ImGui::SetCursorPosX((windowSize.x - mTitleWidth) * 0.5f);
        ImGui::TextColored(ImVec4(0.003f, 0.725f, 0.992f, 1.0f), "%s", menuTitle.c_str());
        
        ImGui::Dummy(ImVec2(0.0f, 40.0f));
        float menuBtnWidth = 340.0f;
        
        ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
        if (ImGui::Button("ESTUDIO DE TIPOS [BD]", ImVec2(menuBtnWidth, 48))) {
            gameState = GameState::TYPE_DATABASE_STUDIO;
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
        if (ImGui::Button("ACTUALIZACIONES DEL SISTEMA", ImVec2(menuBtnWidth, 48))) {
            gameState = GameState::UPDATE_SCREEN;
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
        if (ImGui::Button("SALIR AL TÍTULO", ImVec2(menuBtnWidth, 48))) {
            gameState = GameState::TITLE_SCREEN;
        }
    }
}