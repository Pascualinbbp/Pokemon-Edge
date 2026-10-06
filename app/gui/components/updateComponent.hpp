#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiStyle.hpp"
#include "../../managers/updateManager.hpp"

// Pantalla de actualización: pokéball giratoria, fase actual, barra de progreso y detalle.
// Se muestra mientras UpdateManager esté activo y vuelve sola al título al terminar.
namespace UpdateComponent {
    namespace detail {
        inline constexpr float BAR_WIDTH = 460.0f;
        inline constexpr float BAR_HEIGHT = 18.0f;

        inline const char* phaseText(UpdateManager::Phase phase) {
            switch (phase) {
                case UpdateManager::Phase::CHECKING:    return "Buscando actualizaciones...";
                case UpdateManager::Phase::DOWNLOADING: return "Descargando actualización...";
                case UpdateManager::Phase::INSTALLING:  return "Instalando...";
                case UpdateManager::Phase::RESTARTING:  return "Reiniciando...";
                case UpdateManager::Phase::FAILED:      return "No se pudo actualizar";
                default:                                return "";
            }
        }

        // Contorno de pokéball con el ecuador girando (el mismo estilo que el aviso de autoguardado).
        inline void spinner(ImDrawList* dl, const ImVec2& c, bool spinning) {
            constexpr float RADIUS = 34.0f, BUTTON = 12.0f;
            const float angle = spinning ? static_cast<float>(ImGui::GetTime()) * 4.0f : 0.0f;
            const ImVec2 d(std::cos(angle), std::sin(angle));
            dl->AddCircle(c, RADIUS, IM_COL32(0, 0, 0, 160), 40, 7.0f);
            dl->AddCircle(c, RADIUS, GuiStyle::FOREGROUND, 40, 4.0f);
            dl->AddLine(ImVec2(c.x - d.x * RADIUS, c.y - d.y * RADIUS), ImVec2(c.x - d.x * BUTTON, c.y - d.y * BUTTON), GuiStyle::FOREGROUND, 4.0f);
            dl->AddLine(ImVec2(c.x + d.x * BUTTON, c.y + d.y * BUTTON), ImVec2(c.x + d.x * RADIUS, c.y + d.y * RADIUS), GuiStyle::FOREGROUND, 4.0f);
            dl->AddCircle(c, BUTTON, GuiStyle::FOREGROUND, 20, 4.0f);
        }
    }

    inline void render(GameState& state) {
        const UpdateManager::Snapshot update = UpdateManager::snapshot();
        if (update.phase == UpdateManager::Phase::IDLE) {
            state = GameState::TITLE_SCREEN;
            return;
        }

        const bool failed = update.phase == UpdateManager::Phase::FAILED;
        GuiLayout::beginAt(0.28f);
        GuiLayout::centeredText("ACTUALIZANDO");
        GuiLayout::gap(GuiLayout::GAP_LARGE);

        const ImVec2 p = ImGui::GetCursorScreenPos();
        detail::spinner(ImGui::GetWindowDrawList(), ImVec2(p.x + ImGui::GetWindowSize().x * 0.5f, p.y + 40.0f), !failed);
        ImGui::Dummy(ImVec2(0.0f, 90.0f));

        GuiLayout::centeredText(detail::phaseText(update.phase), failed ? GuiStyle::DANGER : GuiStyle::FOREGROUND);
        GuiLayout::gap(GuiLayout::GAP_SMALL);

        if (!failed) {
            GuiLayout::centerX(detail::BAR_WIDTH);
            if (update.progress >= 0.0f) {
                ImGui::ProgressBar((std::min)(update.progress, 1.0f), ImVec2(detail::BAR_WIDTH, detail::BAR_HEIGHT), "");
                char percent[16];
                std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(std::lround((std::min)(update.progress, 1.0f) * 100.0f)));
                GuiLayout::centeredText(percent, GuiStyle::MUTED);
            } else { // progreso indeterminado: la barra recorre el ancho
                const float t = static_cast<float>(std::fmod(ImGui::GetTime() * 0.6, 1.0));
                ImGui::ProgressBar(t, ImVec2(detail::BAR_WIDTH, detail::BAR_HEIGHT), "");
            }
        }
        if (!update.message.empty()) GuiLayout::centeredText(update.message.c_str(), GuiStyle::MUTED);

        if (failed) {
            GuiLayout::gap(GuiLayout::GAP_LARGE);
            if (GuiLayout::menuButton("CONTINUAR")) UpdateManager::report(UpdateManager::Phase::IDLE);
        }
    }
}
