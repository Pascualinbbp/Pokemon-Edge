#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiPrompts.hpp"
#include "../style/guiStyle.hpp"
#include "../../engine/core/gameStatus.hpp"
#include "../../engine/core/inputDevice.hpp"
#include "../../utils/core/stringUtil.hpp"

namespace HudComponent {
    namespace detail {
        // Cruceta en el centro de la pantalla; aparece con la cámara de apuntado.
        inline void drawCrosshair(ImDrawList* dl, float alpha) {
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const ImVec2 c(size.x * 0.5f, size.y * 0.5f);
            const int a = static_cast<int>(alpha * 255.0f);
            const ImU32 white = IM_COL32(255, 255, 255, a);
            const ImU32 shadow = IM_COL32(0, 0, 0, static_cast<int>(a * 0.6f));
            constexpr float GAP = 6.0f;
            constexpr float LENGTH = 10.0f;

            static constexpr ImVec2 DIRECTIONS[4] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f } };
            for (const ImVec2& d : DIRECTIONS) {
                const ImVec2 from(c.x + d.x * GAP, c.y + d.y * GAP);
                const ImVec2 to(c.x + d.x * (GAP + LENGTH), c.y + d.y * (GAP + LENGTH));
                dl->AddLine(from, to, shadow, 4.0f);
                dl->AddLine(from, to, white, 2.0f);
            }
            dl->AddCircleFilled(c, 2.0f, white);
        }
    }

    inline void render(GameState& state, InputDevice device, const GameStatus& status) {
        ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
        ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

        // Ayudas de controles: el icono cambia solo según el dispositivo en uso.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        pos.y += 6.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::JUMP, "Saltar", device) + 24.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::CROUCH, "Agacharse", device) + 24.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::AIM, "Apuntar", device) + 24.0f;
        pos.x += GuiPrompts::draw(dl, pos, GuiPrompts::Action::THROW, "Lanzar", device) + 24.0f;
        GuiPrompts::draw(dl, pos, GuiPrompts::Action::PAUSE, "Pausa", device);

        ImGui::Dummy(ImVec2(0.0f, 36.0f));
        char buffer[48];
        ImGui::TextUnformatted(StringUtil::formatTo(buffer, "Capturas: %d", status.captures));

        if (status.aimBlend > 0.05f) detail::drawCrosshair(dl, status.aimBlend);

        if (status.captureNotice) {
            GuiLayout::beginAt(0.2f);
            GuiLayout::centeredText("¡CAPTURADO!", GuiStyle::SUCCESS);
        }

        // repeat = false: mantener ESC pulsado no debe alternar el menú. (Con mando, la pausa llega
        // como evento de entrada y la gestiona el bucle principal.)
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) state = GameState::PAUSED;
    }
}
