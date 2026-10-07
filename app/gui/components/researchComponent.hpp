#pragma once
#include <cstdio>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/itemIcon.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/evRules.hpp"
#include "../../engine/world/inventory.hpp"
#include "../../engine/world/pokemonStorage.hpp"
#include "../../models/gameData.hpp"

// Máquina de investigación: analiza los pokémon capturados desde la última vez, revela sus estadísticas y su potencial
// y paga una recompensa en pokémonedas según su rareza (más si son variocolor). Puede liberar sola los que no
// lleguen al potencial mínimo elegido (nunca variocolor ni el inicial).
namespace ResearchComponent {
    namespace detail {
        inline AnalysisReport lastReport;
        inline bool hasReport = false;

        inline ImU32 rankColor(EvRules::Rank rank) {
            switch (rank) {
                case EvRules::Rank::D:      return IM_COL32(120, 120, 130, 255);
                case EvRules::Rank::C:      return IM_COL32(90, 150, 200, 255);
                case EvRules::Rank::B:      return IM_COL32(80, 180, 110, 255);
                case EvRules::Rank::A:      return IM_COL32(230, 170, 50, 255);
                case EvRules::Rank::S:      return IM_COL32(230, 100, 60, 255);
                case EvRules::Rank::S_PLUS: return IM_COL32(200, 70, 200, 255);
            }
            return IM_COL32(120, 120, 130, 255);
        }

        inline void drawReport(const GameData& data, const ImVec2& pos, const ImVec2& size) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y));
            if (!hasReport) {
                GuiCards::centeredText(dl, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), GuiStyle::MUTED, "Aquí aparecerá el resultado del análisis", 0.95f);
                return;
            }

            char text[96];
            std::snprintf(text, sizeof(text), "%d pokémon analizados   +%d pokémonedas", static_cast<int>(lastReport.entries.size()), lastReport.total);
            GuiCards::text(dl, ImVec2(pos.x + 18.0f, pos.y + 14.0f), IM_COL32(255, 215, 90, 255), text, 1.1f);

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 12.0f, pos.y + 48.0f));
            ImGui::BeginChild("##report", ImVec2(size.x - 24.0f, size.y - 60.0f), false);
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            for (const AnalysisEntry& entry : lastReport.entries) {
                const PokemonSpecies* species = data.speciesById(entry.speciesId);
                if (!species) continue;
                const ImVec2 a = ImGui::GetCursorScreenPos();
                const float width = ImGui::GetContentRegionAvail().x;
                cdl->AddRectFilled(a, ImVec2(a.x + width, a.y + 44.0f), IM_COL32(255, 255, 255, entry.released ? 12 : 26), 8.0f);
                ItemIcon::creature(cdl, ImVec2(a.x + 26.0f, a.y + 22.0f), 14.0f, species->id, entry.released ? 0.5f : 1.0f);
                std::snprintf(text, sizeof(text), "%s%s  Nv. %d", entry.shiny ? "* " : "", species->name.c_str(), entry.level);
                GuiCards::text(cdl, ImVec2(a.x + 50.0f, a.y + 13.0f), entry.shiny ? IM_COL32(255, 220, 90, 255) : IM_COL32(255, 255, 255, 240), text, 1.0f);
                float x = a.x + width - 8.0f;
                std::snprintf(text, sizeof(text), "+%d", entry.reward);
                x -= GuiCards::textWidth(text, 0.95f);
                GuiCards::text(cdl, ImVec2(x, a.y + 13.0f), IM_COL32(255, 215, 90, 255), text, 0.95f);
                std::snprintf(text, sizeof(text), "%s", EvRules::label(entry.rank));
                x -= GuiCards::textWidth(text, 0.9f) + 24.0f;
                GuiCards::chip(cdl, ImVec2(x, a.y + 10.0f), text, rankColor(entry.rank), 0.9f);
                if (entry.released) GuiCards::text(cdl, ImVec2(x - 92.0f, a.y + 13.0f), IM_COL32(255, 120, 100, 255), "liberado", 0.9f);
                ImGui::Dummy(ImVec2(0.0f, 50.0f));
            }
            ImGui::EndChild();
        }
    }

    // ESC o el botón de volver cierran la pantalla (vuelven a 'back').
    inline void render(GameState& state, GameState back, const GameData& data, PokemonStorage& storage, Inventory& inventory) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        constexpr float MARGIN = 48.0f;
        const float top = MARGIN + 54.0f;
        const float height = screen.y - top - MARGIN - 28.0f;
        constexpr float SIDE_WIDTH = 380.0f;

        if (GuiCards::backButton(ImVec2(MARGIN, MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(MARGIN + 140.0f, MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "MÁQUINA DE INVESTIGACIÓN", 1.6f);
        char money[48];
        std::snprintf(money, sizeof(money), "Pokémonedas: %d", inventory.money());
        GuiCards::text(dl, ImVec2(screen.x - MARGIN - GuiCards::textWidth(money, 1.2f), MARGIN - 4.0f), IM_COL32(255, 215, 90, 255), money, 1.2f);

        // Columna izquierda: pendientes, potencial mínimo y botón de analizar.
        const ImVec2 sidePos(MARGIN, top);
        GuiCards::panel(dl, sidePos, ImVec2(sidePos.x + SIDE_WIDTH, sidePos.y + height));
        char text[96];
        const int pending = storage.pendingCount();
        std::snprintf(text, sizeof(text), "Pokémon sin analizar: %d", pending);
        GuiCards::text(dl, ImVec2(sidePos.x + 20.0f, sidePos.y + 20.0f), IM_COL32(255, 255, 255, 255), text, 1.1f);
        GuiCards::text(dl, ImVec2(sidePos.x + 20.0f, sidePos.y + 50.0f), GuiStyle::MUTED, "Revela estadísticas y potencial, y paga por cada captura.", 0.8f);

        GuiCards::text(dl, ImVec2(sidePos.x + 20.0f, sidePos.y + 96.0f), IM_COL32(255, 255, 255, 255), "Liberar automáticamente por debajo de:", 0.9f);
        float x = sidePos.x + 20.0f;
        const EvRules::Rank current = storage.autoRelease();
        for (const EvRules::Rank rank : EvRules::RANKS) {
            const bool off = rank == EvRules::Rank::D; // D = no liberar nada
            const char* label = off ? "NO" : EvRules::label(rank);
            const float w = GuiCards::textWidth(label, 0.95f) + 28.0f;
            ImGui::SetCursorScreenPos(ImVec2(x, sidePos.y + 124.0f));
            ImGui::PushID(static_cast<int>(rank));
            if (GuiCards::button("##rank", label, ImVec2(w, 30.0f), true, rank == current ? detail::rankColor(rank) : IM_COL32(70, 70, 92, 255))) storage.setAutoRelease(rank);
            ImGui::PopID();
            x += w + 6.0f;
        }
        GuiCards::text(dl, ImVec2(sidePos.x + 20.0f, sidePos.y + 164.0f), GuiStyle::MUTED, "Nunca libera variocolor ni al pokémon inicial.", 0.8f);

        ImGui::SetCursorScreenPos(ImVec2(sidePos.x + 20.0f, sidePos.y + 210.0f));
        if (GuiCards::button("##analyze", pending > 0 ? "ANALIZAR" : "NADA QUE ANALIZAR", ImVec2(SIDE_WIDTH - 40.0f, 44.0f), pending > 0, IM_COL32(40, 150, 120, 255))) {
            detail::lastReport = storage.analyze();
            detail::hasReport = true;
            inventory.addMoney(detail::lastReport.total);
        }

        detail::drawReport(data, ImVec2(sidePos.x + SIDE_WIDTH + 28.0f, top), ImVec2(screen.x - sidePos.x - SIDE_WIDTH - 28.0f - MARGIN, height));

        GuiCards::text(dl, ImVec2(MARGIN, screen.y - MARGIN), GuiStyle::MUTED, "ESC: volver", 0.9f);
        if (GuiInput::backPressed()) state = back;
    }
}
