#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiPrompts.hpp"
#include "../../engine/core/inputDevice.hpp"

namespace HudComponent {
    inline void render(GameState& state, InputDevice device) {
        ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
        ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

        // Ayudas de controles: el icono cambia solo según el dispositivo en uso.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        pos.y += 6.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::JUMP, "Saltar", device) + 24.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::CROUCH, "Agacharse", device) + 24.0f;
        GuiPrompts::draw(dl, pos, GuiPrompts::Action::PAUSE, "Pausa", device);

        // repeat = false: mantener ESC pulsado no debe alternar el menú. (Con mando, la pausa llega
        // como evento de entrada y la gestiona el bucle principal.)
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) state = GameState::PAUSED;
    }
}