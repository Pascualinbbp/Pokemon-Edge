#pragma once
#include <cstdio>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../../engine/world/state/inventory.hpp"
#include "../../engine/world/state/playerProgress.hpp"

// Menú del jugador (tecla del menú): nivel, experiencia, medallas y pokémonedas, con accesos a la mochila, los
// pokémon, la Pokédex y la tabla de tipos. ESC o el botón de volver cierran la pantalla.
namespace PlayerComponent {
    inline void render(GameState& state, GameState back, const PlayerProgress& progress, const Inventory& inventory) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        constexpr float MARGIN = 48.0f;
        if (GuiCards::backButton(ImVec2(MARGIN, MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(MARGIN + 140.0f, MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "JUGADOR", 1.6f);

        const float width = (std::min)(640.0f, screen.x - MARGIN * 2.0f);
        const ImVec2 pos((screen.x - width) * 0.5f, MARGIN + 60.0f);
        const ImVec2 end(pos.x + width, pos.y + 250.0f);
        GuiCards::panel(dl, pos, end);

        char text[96];
        std::snprintf(text, sizeof(text), "Nivel %d  (máximo %d)", progress.level(), progress.levelCap());
        GuiCards::text(dl, ImVec2(pos.x + 24.0f, pos.y + 20.0f), IM_COL32(255, 255, 255, 255), text, 1.3f);
        const ImVec2 bar(pos.x + 24.0f, pos.y + 62.0f);
        const float barW = width - 48.0f;
        dl->AddRectFilled(bar, ImVec2(bar.x + barW, bar.y + 14.0f), IM_COL32(255, 255, 255, 40), 7.0f);
        dl->AddRectFilled(bar, ImVec2(bar.x + barW * progress.xpFraction(), bar.y + 14.0f), IM_COL32(250, 205, 70, 255), 7.0f);
        if (progress.maxed()) std::snprintf(text, sizeof(text), "Nivel máximo: consigue una medalla para seguir");
        else std::snprintf(text, sizeof(text), "Experiencia: %d / %d", progress.xp(), progress.xpToNext());
        GuiCards::text(dl, ImVec2(bar.x, bar.y + 22.0f), GuiStyle::MUTED, text, 0.9f);

        GuiCards::text(dl, ImVec2(pos.x + 24.0f, pos.y + 118.0f), IM_COL32(255, 255, 255, 255), "Medallas", 1.1f);
        for (int i = 0; i < PlayerProgress::MAX_BADGES; ++i) {
            const ImVec2 c(pos.x + 44.0f + i * ((width - 88.0f) / (PlayerProgress::MAX_BADGES - 1)), pos.y + 178.0f);
            const bool have = i < progress.badges();
            dl->AddCircleFilled(c, 20.0f, have ? IM_COL32(250, 205, 70, 255) : IM_COL32(255, 255, 255, 30), 32);
            dl->AddCircle(c, 20.0f, have ? IM_COL32(255, 240, 170, 255) : IM_COL32(255, 255, 255, 70), 32, 2.0f);
        }
        std::snprintf(text, sizeof(text), "Pokémonedas: %d", inventory.money());
        GuiCards::text(dl, ImVec2(pos.x + 24.0f, pos.y + 214.0f), IM_COL32(255, 215, 90, 255), text, 1.0f);
    }
}
