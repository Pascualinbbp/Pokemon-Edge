#pragma once
#include <algorithm>
#include <cstdio>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiLayout.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../../engine/world/state/playerProgress.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../managers/saveManager.hpp"
#include "../../utils/core/stringUtil.hpp"

// Pantallas con la lista de partidas guardadas (cargar una, o elegir cuál eliminar).
namespace SlotsComponent {
    namespace detail {
        inline constexpr float CARD_WIDTH = 600.0f, CARD_HEIGHT = 96.0f;
        inline constexpr int TEAM_MAX = 6;

        // Ficha de una partida: ranura y fecha, nivel, medallas y Pokédex a la izquierda; el equipo (su sprite y nivel) a la derecha.
        inline void drawCard(ImDrawList* dl, const ImVec2& a, int slot, const SaveSlotInfo& info, bool hovered) {
            const ImVec2 b(a.x + CARD_WIDTH, a.y + CARD_HEIGHT);
            GuiCards::panel(dl, a, b, hovered ? IM_COL32(44, 48, 74, 235) : GuiCards::GLASS);
            char text[96];
            std::snprintf(text, sizeof(text), "PARTIDA %d", slot + 1);
            GuiCards::text(dl, ImVec2(a.x + 18.0f, a.y + 10.0f), IM_COL32(255, 255, 255, 255), text, 1.15f);
            GuiCards::text(dl, ImVec2(a.x + 18.0f, a.y + 10.0f + ImGui::GetFontSize() * 1.15f + 2.0f), GuiStyle::MUTED, info.savedAtText.c_str(), 0.8f);
            if (!info.hasSummary) return; // partida guardada antes de existir el resumen

            const SaveSummary& summary = info.summary;
            std::snprintf(text, sizeof(text), "Nivel %d   Pokédex %d   Monedas %d", summary.playerLevel, summary.dexCaught, summary.money);
            const float lineY = a.y + CARD_HEIGHT - ImGui::GetFontSize() * 0.9f - 24.0f;
            GuiCards::text(dl, ImVec2(a.x + 18.0f, lineY), IM_COL32(230, 232, 245, 255), text, 0.9f);
            for (int i = 0; i < PlayerProgress::MAX_BADGES; ++i) { // medallas: una marca por cada una (llena si se ha conseguido)
                const ImVec2 c(a.x + 26.0f + i * 20.0f, a.y + CARD_HEIGHT - 14.0f);
                if (i < summary.badges) dl->AddCircleFilled(c, 6.5f, IM_COL32(255, 205, 70, 255), 16);
                else dl->AddCircle(c, 6.0f, IM_COL32(255, 255, 255, 80), 16, 1.5f);
            }

            constexpr float R = 24.0f;
            const int count = (std::min)(static_cast<int>(summary.team.size()), TEAM_MAX);
            float x = b.x - 16.0f - count * (R * 2.0f + 8.0f) + 8.0f + R;
            for (int i = 0; i < count; ++i, x += R * 2.0f + 8.0f) {
                const SaveTeamMember& member = summary.team[i];
                const ImVec2 c(x, a.y + CARD_HEIGHT * 0.5f - 8.0f);
                dl->AddCircleFilled(c, R, IM_COL32(60, 66, 100, 235), 32);
                dl->AddCircle(c, R, member.shiny ? IM_COL32(255, 220, 90, 255) : IM_COL32(255, 255, 255, 90), 32, member.shiny ? 2.5f : 1.5f);
                ItemIcon::creature(dl, c, R * 0.62f, member.speciesId);
                std::snprintf(text, sizeof(text), "Nv %d", member.level);
                GuiCards::centeredText(dl, ImVec2(c.x, c.y + R + 11.0f), IM_COL32(230, 232, 245, 255), text, 0.72f);
            }
        }

        // Una ficha por ranura. Devuelve la pulsada o -1.
        inline int slotButtons(bool onlyUsed) {
            int clicked = -1;
            const SaveManager::Slots& slots = SaveManager::slots();
            for (int slot = 0; slot < SaveManager::MAX_SLOTS; ++slot) {
                if (onlyUsed && !slots[slot].used) continue;

                GuiLayout::centerX(CARD_WIDTH);
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                char id[24];
                std::snprintf(id, sizeof(id), "##slot%d", slot);
                if (ImGui::InvisibleButton(id, ImVec2(CARD_WIDTH, CARD_HEIGHT))) clicked = slot;
                drawCard(ImGui::GetWindowDrawList(), origin, slot, slots[slot], ImGui::IsItemHovered());
                GuiLayout::gap(GuiLayout::GAP_SMALL);
            }
            return clicked;
        }
    }

    // "CARGAR PARTIDA": devuelve la ranura elegida o -1.
    inline int renderLoad(GameState& state) {
        GuiLayout::beginAt(0.2f);
        GuiLayout::centeredText("CARGAR PARTIDA");
        GuiLayout::gap(GuiLayout::GAP_LARGE);

        const int slot = detail::slotButtons(true);

        if (GuiInput::backButton()) state = GameState::MAIN_MENU;
        return slot;
    }

    // Con todas las ranuras ocupadas: elegir una y confirmar su eliminación.
    // 'selected' es la ranura pendiente de confirmar (-1 si ninguna). Devuelve la ranura confirmada o -1.
    inline int renderReplace(GameState& state, int& selected) {
        int confirmed = -1;
        const bool back = GuiInput::backPressed();
        char buffer[96];

        GuiLayout::beginAt(0.15f);
        GuiLayout::centeredText(StringUtil::formatTo(buffer, "HAY %d PARTIDAS GUARDADAS", SaveManager::MAX_SLOTS));
        GuiLayout::centeredText("Elige una para eliminarla y empezar la nueva partida", GuiStyle::MUTED);
        GuiLayout::gap(GuiLayout::GAP_LARGE);

        if (selected < 0) {
            const int slot = detail::slotButtons(false);
            if (slot >= 0) selected = slot;

            if (GuiLayout::centeredButton("VOLVER") || back) state = GameState::MAIN_MENU;
        } else {
            GuiLayout::centeredText(StringUtil::formatTo(buffer, "¿Eliminar la partida %d? No se puede deshacer.", selected + 1),
                GuiStyle::DANGER);
            GuiLayout::gap(GuiLayout::GAP_MEDIUM);

            if (GuiLayout::menuButton("ELIMINAR Y EMPEZAR", GuiLayout::GAP_SMALL)) {
                confirmed = selected;
                selected = -1;
            }
            if (GuiLayout::centeredButton("CANCELAR") || back) selected = -1;
        }
        return confirmed;
    }
}
