#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/state/pokemonStorage.hpp"
#include "../../models/gameData.hpp"

// Pokédex: todas las especies en una cuadrícula. Las capturadas se ven con su imagen y una pokéball; las vistas con su imagen
// y una casilla de pokéball vacía; las que no se han visto, oscurecidas y con "???" en sus datos. A la derecha, la ficha.
namespace PokedexComponent {
    namespace detail {
        inline constexpr float MARGIN = 48.0f;
        inline constexpr float DETAIL_WIDTH = 420.0f;
        inline constexpr float TILE_WIDTH = 104.0f;
        inline constexpr float TILE_HEIGHT = 116.0f;
        inline constexpr float GAP = 12.0f;
        inline constexpr float UNSEEN_DARKEN = 0.93f;

        inline int selectedId = -1; // id de la especie seleccionada

        // Marca de la esquina: pokéball (capturado) o casilla vacía (visto).
        inline void marker(ImDrawList* dl, const ImVec2& c, float r, PokemonStorage::DexState state) {
            if (state == PokemonStorage::DEX_CAUGHT) {
                ItemIcon::ball(dl, c, r, { 0.90f, 0.20f, 0.20f });
            } else if (state == PokemonStorage::DEX_SEEN) {
                dl->AddCircle(c, r, IM_COL32(240, 240, 240, 220), 24, 2.0f);
                dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), IM_COL32(240, 240, 240, 140), 1.5f);
                dl->AddCircle(c, r * 0.32f, IM_COL32(240, 240, 240, 140), 14, 1.5f);
            }
        }

        inline void drawGrid(const GameData& data, const PokemonStorage& storage, const ImVec2& size) {
            ImGui::BeginChild("##dexGrid", size, false);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float available = ImGui::GetContentRegionAvail().x;
            const int columns = (std::max)(1, static_cast<int>((available + GAP) / (TILE_WIDTH + GAP)));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(GAP, GAP));
            char number[16];
            for (int i = 0; i < static_cast<int>(data.species.size()); ++i) {
                const PokemonSpecies& species = data.species[i];
                const PokemonStorage::DexState state = storage.dexState(species.id);
                if (i % columns != 0) ImGui::SameLine();
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                std::snprintf(number, sizeof(number), "N.%03d", i + 1);
                ImGui::PushID(species.id);
                const ImU32 tone = state == PokemonStorage::DEX_UNSEEN || species.types.empty() ? IM_COL32(52, 54, 70, 255) : GuiCards::typeColor(species.types.front());
                const bool picked = GuiCards::tile("##dex", ImVec2(TILE_WIDTH, TILE_HEIGHT), species.id == selectedId, tone,
                    [&](ImDrawList* d, const ImVec2& c, float r) { ItemIcon::creature(d, c, r, species.id, 1.0f, state == PokemonStorage::DEX_UNSEEN ? UNSEEN_DARKEN : 0.0f); },
                    state == PokemonStorage::DEX_UNSEEN ? "???" : species.name.c_str(), number, false);
                ImGui::PopID();
                if (picked) selectedId = species.id;
                marker(dl, ImVec2(origin.x + 16.0f, origin.y + 16.0f), 8.0f, state);
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        inline void line(const char* label, const std::string& value) {
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextUnformatted(value.c_str());
        }

        inline void drawDetail(const GameData& data, const PokemonStorage& storage, const PokemonSpecies* species, const ImVec2& pos, const ImVec2& size) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y));
            if (!species) return;

            const PokemonStorage::DexState state = storage.dexState(species->id);
            const bool unseen = state == PokemonStorage::DEX_UNSEEN;
            const bool caught = state == PokemonStorage::DEX_CAUGHT;

            const float headerHeight = 190.0f;
            const ImU32 tone = unseen || species->types.empty() ? IM_COL32(52, 54, 70, 255) : GuiCards::typeColor(species->types.front());
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + headerHeight), GuiCards::withAlpha(tone, 200), GuiCards::ROUNDING, ImDrawFlags_RoundCornersTop);
            const ImVec2 center(pos.x + size.x * 0.5f, pos.y + 78.0f);
            dl->AddCircleFilled(center, 62.0f, IM_COL32(0, 0, 0, 70), 48);
            ItemIcon::creature(dl, center, 46.0f, species->id, 1.0f, unseen ? UNSEEN_DARKEN : 0.0f);
            marker(dl, ImVec2(pos.x + 26.0f, pos.y + 26.0f), 12.0f, state);
            GuiCards::centeredText(dl, ImVec2(center.x, pos.y + 152.0f), IM_COL32(255, 255, 255, 255), unseen ? "???" : species->name.c_str(), 1.25f);
            GuiCards::centeredText(dl, ImVec2(center.x, pos.y + 175.0f), IM_COL32(255, 255, 255, 210),
                                   unseen ? "Sin ver" : caught ? "Capturado" : "Visto, sin capturar", 0.85f);

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f, pos.y + headerHeight + 12.0f));
            ImGui::BeginChild("##dexDetail", ImVec2(size.x - 32.0f, size.y - headerHeight - 24.0f), false);

            if (unseen) {
                GuiCards::muted("Tipos: ???");
                GuiCards::muted("Estadísticas: ???");
                GuiCards::muted("Descripción: ???");
                ImGui::EndChild();
                return;
            }

            ImDrawList* child = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            float x = cursor.x;
            for (const int id : species->types) if (const PokemonType* type = data.type(id)) x += GuiCards::typeChip(child, ImVec2(x, cursor.y), *type) + 6.0f;
            ImGui::Dummy(ImVec2(0.0f, 30.0f));

            GuiCards::muted(species->description.c_str());
            ImGui::Dummy(ImVec2(0.0f, 6.0f));

            const float barWidth = ImGui::GetContentRegionAvail().x - 150.0f;
            char text[24];
            for (int i = 0; i < BaseStats::COUNT; ++i) {
                const ImVec2 at = ImGui::GetCursorScreenPos();
                ImGui::TextUnformatted(BaseStats::name(i));
                std::snprintf(text, sizeof(text), "%d", species->stats.at(i));
                GuiCards::bar(child, ImVec2(at.x + 90.0f, at.y + 6.0f), barWidth, (std::min)(1.0f, species->stats.at(i) / 160.0f), IM_COL32(90, 150, 255, 255));
                GuiCards::text(child, ImVec2(at.x + 98.0f + barWidth, at.y), IM_COL32(255, 255, 255, 255), text);
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
            }

            if (!caught) {
                ImGui::Dummy(ImVec2(0.0f, 8.0f));
                GuiCards::muted("Captúralo para ver dónde vive y cómo evoluciona.");
                ImGui::EndChild();
                return;
            }

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            std::string places;
            for (const HabitatWeight& entry : species->spawn.habitats) {
                for (const Habitat& habitat : data.habitats) {
                    if (habitat.id == entry.habitatId && entry.weight > 0.0f) places += (places.empty() ? "" : ", ") + habitat.name;
                }
            }
            line("Hábitats:", places.empty() ? "no aparece salvaje" : places);
            std::snprintf(text, sizeof(text), "%d", species->catchRate);
            line("Ratio de captura:", text);
            for (const Evolution& evolution : species->evolutions) {
                const PokemonSpecies* next = data.speciesById(evolution.toId);
                const bool known = next && storage.dexState(next->id) != PokemonStorage::DEX_UNSEEN;
                std::string how;
                switch (evolution.method) {
                    case Evolution::Method::LEVEL:     how = "nivel " + std::to_string(evolution.level); break;
                    case Evolution::Method::STONE:     how = "mineral"; break;
                    case Evolution::Method::FAINT:     how = "derrotado en combate (nivel " + std::to_string(evolution.level) + ")"; break;
                    case Evolution::Method::WATERFALL: how = "remontando una cascada"; break;
                }
                line("Evoluciona:", std::string(known ? next->name : "???") + "  (" + how + ")");
            }
            ImGui::EndChild();
        }
    }

    // ESC, P, Select o el botón de volver del mando cierran la Pokédex.
    inline void render(GameState& state, GameState back, const GameData& data, const PokemonStorage& storage) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));
        if (data.species.empty()) {
            state = back;
            return;
        }

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float top = detail::MARGIN + 54.0f;
        const float height = screen.y - top - detail::MARGIN - 28.0f;

        if (GuiCards::backButton(ImVec2(detail::MARGIN, detail::MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(detail::MARGIN + 140.0f, detail::MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "POKÉDEX", 1.6f);
        char counts[96];
        std::snprintf(counts, sizeof(counts), "Vistos: %d     Capturados: %d     Total: %d", storage.dexCount(PokemonStorage::DEX_SEEN),
                      storage.dexCount(PokemonStorage::DEX_CAUGHT), static_cast<int>(data.species.size()));
        GuiCards::text(dl, ImVec2(screen.x - detail::MARGIN - GuiCards::textWidth(counts, 1.1f), detail::MARGIN - 2.0f), IM_COL32(255, 215, 90, 255), counts, 1.1f);

        const PokemonSpecies* picked = data.speciesById(detail::selectedId);
        if (!picked) {
            picked = &data.species.front();
            detail::selectedId = picked->id;
        }

        const ImVec2 gridPos(detail::MARGIN, top);
        const float gridWidth = screen.x - detail::MARGIN * 2.0f - detail::DETAIL_WIDTH - 28.0f;
        ImGui::SetCursorScreenPos(gridPos);
        detail::drawGrid(data, storage, ImVec2(gridWidth, height));
        detail::drawDetail(data, storage, picked, ImVec2(screen.x - detail::MARGIN - detail::DETAIL_WIDTH, top), ImVec2(detail::DETAIL_WIDTH, height));

        GuiCards::text(dl, ImVec2(detail::MARGIN, screen.y - detail::MARGIN), GuiStyle::MUTED, "ESC: volver", 0.9f);
        if (GuiInput::backPressed() || ImGui::IsKeyPressed(ImGuiKey_P, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight, false)) state = back;
    }
}
