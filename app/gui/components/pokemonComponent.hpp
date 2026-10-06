#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/itemIcon.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/pokemonStorage.hpp"
#include "../../models/gameData.hpp"

// Gestión de pokémon: el equipo (6) a la izquierda, el PC en el centro y la ficha del seleccionado a la derecha
// (tipos, estadísticas base, habilidades de combate y de recolección).
namespace PokemonComponent {
    namespace detail {
        inline constexpr float MARGIN = 48.0f;
        inline constexpr float TEAM_WIDTH = 310.0f;
        inline constexpr float DETAIL_WIDTH = 430.0f;
        inline constexpr float SLOT_HEIGHT = 84.0f;
        inline constexpr float TILE_WIDTH = 96.0f;
        inline constexpr float TILE_HEIGHT = 108.0f;
        inline constexpr float GAP = 12.0f;
        inline constexpr float MAX_STAT = 130.0f; // la base más alta que cabe en el gráfico hexagonal

        struct Selection {
            bool inTeam = true;
            int index = 0;
        };
        inline Selection selection;

        inline void muted(const char* value) {
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
            ImGui::TextWrapped("%s", value);
            ImGui::PopStyleColor();
        }

        inline void heading(const char* value) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::FOREGROUND);
            ImGui::TextUnformatted(value);
            ImGui::PopStyleColor();
        }

        // Color de fondo de un pokémon: el de su primer tipo.
        inline ImU32 tone(const GameData& data, const PokemonSpecies& species) {
            if (!species.elements.empty()) return GuiCards::withAlpha(GuiCards::elementColor(species.elements.front()), 190);
            return GuiStyle::SURFACE;
        }

        // Fichas de tipo en fila desde 'pos'. Devuelve el ancho ocupado.
        inline float elementChips(ImDrawList* dl, const GameData& data, const PokemonSpecies& species, ImVec2 pos) {
            float used = 0.0f;
            for (const int id : species.elements) {
                if (const Element* element = data.element(id)) {
                    const float w = GuiCards::elementChip(dl, pos, *element);
                    pos.x += w + 6.0f;
                    used += w + 6.0f;
                }
            }
            return used;
        }

        inline const PokemonSpecies* speciesOf(const GameData& data, const OwnedPokemon& owned) { return data.speciesById(owned.speciesId); }

        // Casilla del equipo: icono, nombre, tipos y marca de líder.
        inline bool teamSlot(const GameData& data, const OwnedPokemon* owned, int index, bool selected, float width) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 a = ImGui::GetCursorScreenPos();
            const ImVec2 b(a.x + width, a.y + SLOT_HEIGHT);
            ImGui::PushID(index);
            const bool clicked = ImGui::InvisibleButton("##slot", ImVec2(width, SLOT_HEIGHT));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            const PokemonSpecies* species = owned ? speciesOf(data, *owned) : nullptr;
            if (!species) {
                dl->AddRect(a, b, IM_COL32(255, 255, 255, 45), 12.0f, 0, 1.5f);
                GuiCards::centeredText(dl, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), GuiStyle::MUTED, "Vacío", 0.9f);
                return false;
            }
            dl->AddRectFilled(a, b, hovered ? GuiCards::withAlpha(tone(data, *species), 240) : tone(data, *species), 12.0f);
            ItemIcon::creature(dl, ImVec2(a.x + 44.0f, a.y + SLOT_HEIGHT * 0.5f), 26.0f, species->id);
            GuiCards::text(dl, ImVec2(a.x + 84.0f, a.y + 12.0f), IM_COL32(255, 255, 255, 255), species->name.c_str(), 1.15f);
            elementChips(dl, data, *species, ImVec2(a.x + 84.0f, a.y + 46.0f));
            if (index == 0) GuiCards::chip(dl, ImVec2(b.x - 62.0f, a.y + 8.0f), "Líder", IM_COL32(0, 0, 0, 140), 0.8f);
            if (selected) dl->AddRect(a, b, GuiCards::SELECT, 12.0f, 0, 3.0f);
            return clicked;
        }

        inline void drawTeam(const GameData& data, const PokemonStorage& storage, const ImVec2& pos, float height) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            char header[48];
            std::snprintf(header, sizeof(header), "EQUIPO  %d / %d", static_cast<int>(storage.team().size()), PokemonStorage::TEAM_SIZE);
            GuiCards::text(dl, pos, IM_COL32(255, 255, 255, 255), header, 1.1f);

            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + 34.0f));
            ImGui::BeginChild("##team", ImVec2(TEAM_WIDTH, height - 34.0f), false, ImGuiWindowFlags_NoScrollbar);
            for (int i = 0; i < PokemonStorage::TEAM_SIZE; ++i) {
                const bool filled = i < static_cast<int>(storage.team().size());
                if (teamSlot(data, filled ? &storage.team()[i] : nullptr, i, selection.inTeam && selection.index == i, TEAM_WIDTH) && filled) {
                    selection = { true, i };
                }
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
            }
            ImGui::EndChild();
        }

        inline void drawBox(const GameData& data, const PokemonStorage& storage, const ImVec2& pos, const ImVec2& size) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            char header[48];
            std::snprintf(header, sizeof(header), "PC  %d / %d", static_cast<int>(storage.pc().size()), PokemonStorage::PC_CAPACITY);
            GuiCards::text(dl, pos, IM_COL32(255, 255, 255, 255), header, 1.1f);

            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + 34.0f));
            ImGui::BeginChild("##box", ImVec2(size.x, size.y - 34.0f), false);
            if (storage.pc().empty()) muted("El PC está vacío. Los pokémon capturados con el equipo lleno llegan aquí.");

            const int columns = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x + GAP) / (TILE_WIDTH + GAP)));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(GAP, GAP));
            for (int i = 0; i < static_cast<int>(storage.pc().size()); ++i) {
                const PokemonSpecies* species = speciesOf(data, storage.pc()[i]);
                if (!species) continue;
                if (i % columns != 0) ImGui::SameLine();
                ImGui::PushID(i);
                const bool picked = GuiCards::tile("##pc", ImVec2(TILE_WIDTH, TILE_HEIGHT), !selection.inTeam && selection.index == i,
                    tone(data, *species), [&](ImDrawList* d, const ImVec2& c, float r) { ItemIcon::creature(d, c, r, species->id); },
                    species->name.c_str());
                ImGui::PopID();
                if (picked) selection = { false, i };
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        // Gráfico hexagonal de estadísticas base con sus valores.
        inline void drawStats(const BaseStats& stats) {
            struct Row { const char* label; int value; };
            const Row rows[6] = { { "PS", stats.hp }, { "Ataque", stats.attack }, { "Defensa", stats.defense },
                                  { "Velocidad", stats.speed }, { "Def. esp.", stats.spDefense }, { "At. esp.", stats.spAttack } };
            float values[6];
            for (int i = 0; i < 6; ++i) values[i] = static_cast<float>(rows[i].value) / MAX_STAT;

            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            const float radius = 64.0f;
            const ImVec2 center(origin.x + width * 0.5f, origin.y + radius + 30.0f);
            GuiCards::radar(dl, center, radius, values, GuiStyle::ACCENT);

            constexpr float PI = 3.14159265f;
            char text[32];
            for (int i = 0; i < 6; ++i) {
                const float angle = -PI * 0.5f + static_cast<float>(i) * PI / 3.0f;
                std::snprintf(text, sizeof(text), "%s %d", rows[i].label, rows[i].value);
                const float w = GuiCards::textWidth(text, 0.85f);
                const float cx = std::cos(angle), cy = std::sin(angle);
                const ImVec2 at(center.x + cx * (radius + 14.0f) + (cx > 0.3f ? 0.0f : cx < -0.3f ? -w : -w * 0.5f),
                                center.y + cy * (radius + 14.0f) - ImGui::GetFontSize() * 0.4f);
                GuiCards::text(dl, at, IM_COL32(255, 255, 255, 230), text, 0.85f);
            }
            ImGui::Dummy(ImVec2(0.0f, radius * 2.0f + 60.0f));
        }

        inline void drawAbilities(const GameData& data, PokemonStorage& storage, const PokemonSpecies& species, const OwnedPokemon& owned) {
            heading("Habilidades de combate");
            for (const SpeciesAbility& entry : species.abilities) {
                if (!entry.passive) continue;
                if (const Ability* ability = data.ability(entry.abilityId)) {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    const ImVec2 at = ImGui::GetCursorScreenPos();
                    const float w = GuiCards::chip(dl, at, ability->name.c_str(), IM_COL32(130, 80, 190, 255));
                    GuiCards::text(dl, ImVec2(at.x + w + 8.0f, at.y + 2.0f), GuiStyle::MUTED, "pasiva (siempre activa)", 0.85f);
                    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() + 10.0f));
                    muted(ability->description.c_str());
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::TextUnformatted("Activa:");
            for (const SpeciesAbility& entry : species.abilities) {
                if (entry.passive) continue;
                if (const Ability* ability = data.ability(entry.abilityId)) {
                    const bool equipped = entry.abilityId == owned.abilityId;
                    ImGui::PushID(entry.abilityId);
                    if (GuiCards::button("##ability", ability->name.c_str(), ImVec2(GuiCards::textWidth(ability->name.c_str(), 0.95f) + 28.0f, 30.0f), true,
                                         equipped ? GuiStyle::ACCENT : IM_COL32(70, 70, 88, 255))) {
                        storage.setAbility(selection.inTeam, selection.index, entry.abilityId);
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                }
            }
            ImGui::NewLine();
            if (const Ability* ability = data.ability(owned.abilityId)) muted(ability->description.c_str());
        }

        // Habilidades de recolección con su nivel y los recursos que permiten trabajar.
        inline void drawGathering(const GameData& data, const PokemonSpecies& species) {
            heading("Recolección");
            if (species.skills.empty()) {
                muted("Este pokémon no sabe recolectar.");
                return;
            }
            char text[160];
            for (const SpeciesSkill& entry : species.skills) {
                const Skill* skill = data.skill(entry.skillId);
                std::string works;
                for (const ResourceNodeType& node : data.nodes) {
                    if (node.skillId == entry.skillId && node.level <= entry.level) works += (works.empty() ? "" : ", ") + node.name;
                }
                std::snprintf(text, sizeof(text), "%s nivel %d", skill ? skill->name.c_str() : "?", entry.level);
                ImGui::TextUnformatted(text);
                if (!works.empty()) muted(("Puede trabajar: " + works).c_str());
            }
        }

        inline void drawDetail(const GameData& data, PokemonStorage& storage, const ImVec2& pos, const ImVec2& size) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y));

            const std::vector<OwnedPokemon>& list = selection.inTeam ? storage.team() : storage.pc();
            if (selection.index < 0 || selection.index >= static_cast<int>(list.size())) {
                GuiCards::centeredText(dl, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), GuiStyle::MUTED, "Selecciona un pokémon", 0.95f);
                return;
            }
            const OwnedPokemon owned = list[selection.index];
            const PokemonSpecies* species = speciesOf(data, owned);
            if (!species) return;

            // Cabecera con el color de su tipo.
            const float headerHeight = 150.0f;
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + headerHeight), tone(data, *species), GuiCards::ROUNDING, ImDrawFlags_RoundCornersTop);
            ItemIcon::creature(dl, ImVec2(pos.x + 72.0f, pos.y + 66.0f), 44.0f, species->id);
            GuiCards::text(dl, ImVec2(pos.x + 142.0f, pos.y + 30.0f), IM_COL32(255, 255, 255, 255), species->name.c_str(), 1.5f);
            elementChips(dl, data, *species, ImVec2(pos.x + 142.0f, pos.y + 72.0f));
            GuiCards::text(dl, ImVec2(pos.x + 142.0f, pos.y + 104.0f), IM_COL32(255, 255, 255, 200),
                           selection.inTeam ? (selection.index == 0 ? "Líder del equipo" : "En el equipo") : "En el PC", 0.85f);

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f, pos.y + headerHeight + 10.0f));
            ImGui::BeginChild("##detail", ImVec2(size.x - 32.0f, size.y - headerHeight - 74.0f), false);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(species->description.c_str());
            ImGui::PopTextWrapPos();
            drawStats(species->stats);
            drawAbilities(data, storage, *species, owned);
            drawGathering(data, *species);
            ImGui::EndChild();

            // Acciones
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f, pos.y + size.y - 54.0f));
            if (selection.inTeam) {
                if (GuiCards::button("##lead", "HACER LÍDER", ImVec2(180.0f, 38.0f), selection.index > 0)) {
                    storage.makeLead(selection.index);
                    selection.index = 0;
                }
                ImGui::SameLine();
                if (GuiCards::button("##sendPc", "ENVIAR AL PC", ImVec2(180.0f, 38.0f), !storage.pcFull() && storage.team().size() > 1)) {
                    storage.sendToPc(selection.index);
                    selection.index = (std::max)(0, (std::min)(selection.index, static_cast<int>(storage.team().size()) - 1));
                }
            } else if (GuiCards::button("##sendTeam", "AÑADIR AL EQUIPO", ImVec2(220.0f, 38.0f), !storage.teamFull())) {
                storage.sendToTeam(selection.index);
                selection.index = (std::max)(0, (std::min)(selection.index, static_cast<int>(storage.pc().size()) - 1));
            }
        }
    }

    // ESC o el botón de volver cierran la pantalla (vuelven a 'back').
    inline void render(GameState& state, GameState back, const GameData& data, PokemonStorage& storage) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));

        // La selección debe seguir apuntando a algo que existe.
        const std::vector<OwnedPokemon>& current = detail::selection.inTeam ? storage.team() : storage.pc();
        if (detail::selection.index >= static_cast<int>(current.size())) detail::selection = { true, 0 };

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float top = detail::MARGIN + 54.0f;
        const float height = screen.y - top - detail::MARGIN - 28.0f;

        GuiCards::text(dl, ImVec2(detail::MARGIN, detail::MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "POKÉMON", 1.6f);

        detail::drawTeam(data, storage, ImVec2(detail::MARGIN, top), height);

        const float boxX = detail::MARGIN + detail::TEAM_WIDTH + 36.0f;
        const float boxWidth = screen.x - boxX - detail::DETAIL_WIDTH - detail::MARGIN - 28.0f;
        detail::drawBox(data, storage, ImVec2(boxX, top), ImVec2(boxWidth, height));

        detail::drawDetail(data, storage, ImVec2(screen.x - detail::MARGIN - detail::DETAIL_WIDTH, top), ImVec2(detail::DETAIL_WIDTH, height));

        GuiCards::text(dl, ImVec2(detail::MARGIN, screen.y - detail::MARGIN), GuiStyle::MUTED, "ESC: volver", 0.9f);

        if (GuiInput::backPressed()) state = back;
    }
}
