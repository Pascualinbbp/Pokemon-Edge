// C:\Users\bran\Desktop\Pokemon Edge\app\gui\windows\titleWindow.hpp
#pragma once
#include "imgui.h"
#include "appState.hpp"
#include <d3d11.h>
#include <string>

namespace TitleWindow {
    inline void render(GameState& gameState, ID3D11ShaderResourceView* logoTexture, int logoW, int logoH) {
        ImVec2 windowSize = ImGui::GetWindowSize();

        ImGui::SetCursorPosY(windowSize.y * 0.15f);
        if (logoTexture) {
            float renderW = 320.0f;
            float renderH = (logoH > 0 && logoW > 0) ? (renderW * ((float)logoH / (float)logoW)) : 120.0f;
            ImGui::SetCursorPosX((windowSize.x - renderW) * 0.5f);
            ImGui::Image((void*)logoTexture, ImVec2(renderW, renderH));
        } else {
            std::string titleBig = "P O K E M O N   E D G E";
            float tWidth = ImGui::CalcTextSize(titleBig.c_str()).x;
            ImGui::SetCursorPosX((windowSize.x - tWidth) * 0.5f);
            ImGui::TextColored(ImVec4(0.003f, 0.725f, 0.992f, 1.0f), "%s", titleBig.c_str());
        }

        ImGui::SetCursorPosY(windowSize.y * 0.65f);
        std::string pressText = "- PRESIONA PARA ACCEDER AL ESTUDIO -";
        float pressWidth = ImGui::CalcTextSize(pressText.c_str()).x;
        ImGui::SetCursorPosX((windowSize.x - pressWidth) * 0.5f);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "%s", pressText.c_str());

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        float btnWidth = 260.0f;
        ImGui::SetCursorPosX((windowSize.x - btnWidth) * 0.5f);
        if (ImGui::Button("INICIAR MOTOR", ImVec2(btnWidth, 48))) {
            gameState = GameState::MAIN_MENU;
        }
    }
}