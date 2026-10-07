#pragma once
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/itemIcon.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/evRules.hpp"
#include "../../engine/world/inventory.hpp"
#include "../../engine/world/playerProgress.hpp"
#include "../../engine/world/pokemonStorage.hpp"
#include "../../models/gameData.hpp"

// Gestión de pokémon: el equipo (6) a la izquierda, el PC en el centro y la ficha del seleccionado a la derecha.
// Aquí se entrenan (caramelos y vitaminas) y se liberan (uno o varios). Sus estadísticas y su potencial salen de la
// máquina de investigación: hasta entonces están ocultos.
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
        inline bool selecting = false;          // modo de selección múltiple para liberar
        inline std::set<int> teamMarks, pcMarks; // pokémon marcados
        inline bool confirmRelease = false;     // primer clic en LIBERAR: pide confirmación
        inline std::string message;             // resultado del último entrenamiento (nivel, evolución...)

        inline void clearTransient() {
            confirmRelease = false;
            message.clear();
        }

        inline void select(bool inTeam, int index) {
            selection = { inTeam, index };
            clearTransient();
        }

        inline void clearMarks() {
            teamMarks.clear();
            pcMarks.clear();
            confirmRelease = false;
        }

        inline int markCount() { return static_cast<int>(teamMarks.size() + pcMarks.size()); }

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
        inline ImU32 tone(const PokemonSpecies& species) {
            if (!species.types.empty()) return GuiCards::withAlpha(GuiCards::typeColor(species.types.front()), 190);
            return GuiStyle::SURFACE;
        }

        // Fichas de tipo en fila desde 'pos' (los tipos salen de la tabla type).
        inline void typeChips(ImDrawList* dl, const GameData& data, const PokemonSpecies& species, ImVec2 pos) {
            for (const int id : species.types) {
                if (const PokemonType* type = data.type(id)) pos.x += GuiCards::typeChip(dl, pos, *type) + 6.0f;
            }
        }

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

        inline const PokemonSpecies* speciesOf(const GameData& data, const OwnedPokemon& owned) { return data.speciesById(owned.speciesId); }

        inline std::string title(const PokemonSpecies& species, const OwnedPokemon& owned) { return (owned.shiny ? "* " : "") + species.name; }

        // Marca de selección múltiple sobre la esquina de una casilla.
        inline void markBox(ImDrawList* dl, const ImVec2& corner, bool marked, bool allowed) {
            const ImVec2 a(corner.x - 24.0f, corner.y + 6.0f), b(corner.x - 6.0f, corner.y + 24.0f);
            dl->AddRectFilled(a, b, allowed ? IM_COL32(0, 0, 0, 160) : IM_COL32(0, 0, 0, 60), 4.0f);
            dl->AddRect(a, b, IM_COL32(255, 255, 255, allowed ? 230 : 80), 4.0f, 0, 1.5f);
            if (marked) dl->AddRectFilled(ImVec2(a.x + 4.0f, a.y + 4.0f), ImVec2(b.x - 4.0f, b.y - 4.0f), IM_COL32(255, 120, 90, 255), 2.0f);
        }

        // Casilla del equipo: icono, nombre, nivel, tipos y marca de líder.
        inline bool teamSlot(const GameData& data, const PokemonStorage& storage, const OwnedPokemon* owned, int index, bool selected, float width) {
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
            dl->AddRectFilled(a, b, hovered ? GuiCards::withAlpha(tone(*species), 240) : tone(*species), 12.0f);
            ItemIcon::creature(dl, ImVec2(a.x + 44.0f, a.y + SLOT_HEIGHT * 0.5f), 26.0f, species->id);
            GuiCards::text(dl, ImVec2(a.x + 84.0f, a.y + 10.0f), IM_COL32(255, 255, 255, 255), title(*species, *owned).c_str(), 1.1f);
            char level[16];
            std::snprintf(level, sizeof(level), "Nv. %d", owned->level);
            GuiCards::text(dl, ImVec2(a.x + 84.0f, a.y + 30.0f), IM_COL32(255, 255, 255, 210), level, 0.85f);
            typeChips(dl, data, *species, ImVec2(a.x + 84.0f, a.y + 54.0f));
            if (index == 0 && !selecting) GuiCards::chip(dl, ImVec2(b.x - 62.0f, a.y + 8.0f), "Líder", IM_COL32(0, 0, 0, 140), 0.8f);
            if (selecting) markBox(dl, ImVec2(b.x, a.y), teamMarks.count(index) > 0, !storage.bound(*owned));
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
                if (teamSlot(data, storage, filled ? &storage.team()[i] : nullptr, i, selection.inTeam && selection.index == i, TEAM_WIDTH) && filled) {
                    if (selecting) {
                        if (!storage.bound(storage.team()[i]) && !teamMarks.erase(i)) teamMarks.insert(i);
                        confirmRelease = false;
                    } else select(true, i);
                }
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
            }
            ImGui::EndChild();
        }

        // Cabecera del PC: título y los controles de selección múltiple para liberar.
        inline void drawBoxHeader(PokemonStorage& storage, const ImVec2& pos, float width) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            char header[48];
            std::snprintf(header, sizeof(header), "PC  %d / %d", static_cast<int>(storage.pc().size()), PokemonStorage::PC_CAPACITY);
            GuiCards::text(dl, pos, IM_COL32(255, 255, 255, 255), header, 1.1f);

            float x = pos.x + width;
            if (selecting) {
                char text[40];
                std::snprintf(text, sizeof(text), confirmRelease ? "¿SEGURO? (%d)" : "LIBERAR (%d)", markCount());
                const float w = GuiCards::textWidth(text, 0.95f) + 36.0f;
                x -= w;
                ImGui::SetCursorScreenPos(ImVec2(x, pos.y - 6.0f));
                if (GuiCards::button("##releaseMarked", text, ImVec2(w, 30.0f), markCount() > 0, IM_COL32(190, 50, 50, 255))) {
                    if (confirmRelease) {
                        storage.release(std::vector<int>(teamMarks.begin(), teamMarks.end()), std::vector<int>(pcMarks.begin(), pcMarks.end()));
                        clearMarks();
                        select(true, 0);
                    } else confirmRelease = true;
                }
                x -= 8.0f;
            }
            const char* label = selecting ? "CANCELAR" : "SELECCIONAR";
            const float w = GuiCards::textWidth(label, 0.95f) + 36.0f;
            x -= w;
            ImGui::SetCursorScreenPos(ImVec2(x, pos.y - 6.0f));
            if (GuiCards::button("##selectMode", label, ImVec2(w, 30.0f), true, IM_COL32(70, 70, 92, 255))) {
                selecting = !selecting;
                clearMarks();
            }
        }

        inline void drawBox(const GameData& data, PokemonStorage& storage, const ImVec2& pos, const ImVec2& size) {
            drawBoxHeader(storage, pos, size.x);

            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + 34.0f));
            ImGui::BeginChild("##box", ImVec2(size.x, size.y - 34.0f), false);
            if (storage.pc().empty()) muted("El PC está vacío. Los pokémon capturados con el equipo lleno llegan aquí.");

            const int columns = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x + GAP) / (TILE_WIDTH + GAP)));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(GAP, GAP));
            char badge[16];
            for (int i = 0; i < static_cast<int>(storage.pc().size()); ++i) {
                const OwnedPokemon& owned = storage.pc()[i];
                const PokemonSpecies* species = speciesOf(data, owned);
                if (!species) continue;
                if (i % columns != 0) ImGui::SameLine();
                ImGui::PushID(i);
                std::snprintf(badge, sizeof(badge), "Nv %d", owned.level);
                const bool picked = GuiCards::tile("##pc", ImVec2(TILE_WIDTH, TILE_HEIGHT), !selection.inTeam && selection.index == i,
                    tone(*species), [&](ImDrawList* d, const ImVec2& c, float r) { ItemIcon::creature(d, c, r, species->id); },
                    title(*species, owned).c_str(), badge);
                if (selecting) markBox(ImGui::GetWindowDrawList(), ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y), pcMarks.count(i) > 0, !storage.bound(owned));
                ImGui::PopID();
                if (!picked) continue;
                if (selecting) {
                    if (!storage.bound(owned) && !pcMarks.erase(i)) pcMarks.insert(i);
                    confirmRelease = false;
                } else select(false, i);
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        // Gráfico hexagonal: estadísticas base (azul) y, encima, su crecimiento por EVs (naranja). En cada vértice:
        // "Nombre base" y "EV actual / EV máximo con el que salió".
        inline void drawStats(const BaseStats& stats, const EvRules::Evs& evs, const EvRules::Evs& caps) {
            constexpr int ORDER[6] = { 0, 1, 3, 5, 4, 2 }; // vértices de arriba en sentido horario (índices de BaseStats)
            float base[6], grown[6];
            for (int i = 0; i < 6; ++i) {
                base[i] = static_cast<float>(stats.at(ORDER[i])) / MAX_STAT;
                grown[i] = static_cast<float>(stats.at(ORDER[i]) + evs[ORDER[i]]) / MAX_STAT;
            }

            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            const float radius = 64.0f;
            const ImVec2 center(origin.x + width * 0.5f, origin.y + radius + 34.0f);
            GuiCards::radar(dl, center, radius, base, GuiStyle::ACCENT, &grown, IM_COL32(255, 160, 40, 255));

            constexpr float PI = 3.14159265f;
            char line[32];
            for (int i = 0; i < 6; ++i) {
                const float angle = -PI * 0.5f + static_cast<float>(i) * PI / 3.0f;
                const float cx = std::cos(angle), cy = std::sin(angle);
                const int stat = ORDER[i];
                std::snprintf(line, sizeof(line), "%s %d", BaseStats::name(stat), stats.at(stat));
                char potential[16];
                std::snprintf(potential, sizeof(potential), "%d/%d", evs[stat], caps[stat]);
                const float w = (std::max)(GuiCards::textWidth(line, 0.85f), GuiCards::textWidth(potential, 0.85f));
                const float x = center.x + cx * (radius + 14.0f) + (cx > 0.3f ? 0.0f : cx < -0.3f ? -w : -w * 0.5f);
                const float y = center.y + cy * (radius + 16.0f) - ImGui::GetFontSize() * (cy < -0.3f ? 1.3f : 0.6f);
                GuiCards::text(dl, ImVec2(x, y), IM_COL32(255, 255, 255, 230), line, 0.85f);
                GuiCards::text(dl, ImVec2(x, y + ImGui::GetFontSize() * 0.85f), IM_COL32(255, 170, 60, 255), potential, 0.85f);
            }
            ImGui::Dummy(ImVec2(0.0f, radius * 2.0f + 72.0f));
        }

        // Habilidad activa y pasiva (si la tiene) con su descripción.
        inline void abilityLine(ImDrawList* dl, const Ability* ability, const char* kind, ImU32 color) {
            if (!ability) return;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float w = GuiCards::chip(dl, at, ability->name.c_str(), color);
            GuiCards::text(dl, ImVec2(at.x + w + 8.0f, at.y + 2.0f), GuiStyle::MUTED, kind, 0.85f);
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() + 10.0f));
            muted(ability->description.c_str());
        }

        inline void drawAbilities(const GameData& data, const PokemonSpecies& species) {
            heading("Habilidades");
            ImDrawList* dl = ImGui::GetWindowDrawList();
            abilityLine(dl, data.ability(species.abilityId), "activa", IM_COL32(190, 80, 60, 255));
            if (species.passiveId >= 0) abilityLine(dl, data.ability(species.passiveId), "pasiva", IM_COL32(130, 80, 190, 255));
            else muted("Sin habilidad pasiva.");
        }

        // Habilidad del mundo con su nivel y los recursos que permite trabajar; cualquier pokémon recoge bayas y objetos.
        inline void drawGathering(const GameData& data, const PokemonSpecies& species) {
            heading("Habilidad en el mundo");
            const Skill* skill = data.skill(species.skillId);
            char text[96];
            std::snprintf(text, sizeof(text), "%s nivel %d", skill ? skill->name.c_str() : "?", species.skillLevel);
            ImGui::TextUnformatted(text);
            std::string works;
            for (const ResourceNodeType& node : data.nodes) {
                if (node.skillId == species.skillId && node.level <= species.skillLevel) works += (works.empty() ? "" : ", ") + node.name;
            }
            if (!works.empty()) muted(("Puede trabajar: " + works).c_str());
            muted("Cualquier pokémon recoge bayas maduras y objetos sueltos.");
        }

        // Entrenamiento: caramelo (sube un nivel, hasta el límite del jugador) y vitaminas (EVs de la estadística elegida).
        inline void drawTraining(const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress,
                                 const OwnedPokemon& owned, const PokemonSpecies& species) {
            heading("Entrenamiento");
            char text[96];

            if (const TrainingItem* candy = data.trainingWith(TrainingItem::Effect::LEVEL)) if (const Item* item = data.itemOf(ItemCategory::TRAINING, candy->id)) {
                const int count = inventory.count(item->id);
                std::snprintf(text, sizeof(text), "%s  x%d", candy->name.c_str(), count);
                ImGui::TextUnformatted(text);
                ImGui::SameLine();
                const bool atCap = owned.level >= progress.levelCap();
                if (GuiCards::button("##candy", atCap ? "NIVEL MÁXIMO" : "SUBIR NIVEL", ImVec2(150.0f, 30.0f), count > 0 && !atCap)) {
                    inventory.add(item->id, -1);
                    const std::string before = species.name;
                    const PokemonStorage::LevelResult result = storage.levelUp(selection.inTeam, selection.index, progress.levelCap());
                    const PokemonSpecies* after = data.speciesById((selection.inTeam ? storage.team() : storage.pc())[selection.index].speciesId);
                    message = result == PokemonStorage::LevelResult::EVOLVED && after ? "¡" + before + " ha evolucionado a " + after->name + "!" : "";
                }
            }

            if (const TrainingItem* vitamin = data.trainingWith(TrainingItem::Effect::EV)) if (const Item* item = data.itemOf(ItemCategory::TRAINING, vitamin->id)) {
                const int count = inventory.count(item->id);
                std::snprintf(text, sizeof(text), "%s  x%d  (+%d EV)", vitamin->name.c_str(), count, vitamin->amount);
                ImGui::TextUnformatted(text);
                if (!owned.analyzed) {
                    muted("Analiza el pokémon en la máquina de investigación para entrenar sus EVs.");
                    return;
                }
                for (int stat = 0; stat < BaseStats::COUNT; ++stat) {
                    if (stat % 3 != 0) ImGui::SameLine();
                    ImGui::PushID(stat);
                    if (GuiCards::button("##ev", BaseStats::name(stat), ImVec2(120.0f, 28.0f), count > 0 && owned.evs[stat] < owned.evCaps[stat])) {
                        if (storage.train(selection.inTeam, selection.index, stat, vitamin->amount)) inventory.add(item->id, -1);
                    }
                    ImGui::PopID();
                }
            }
        }

        inline void drawDetail(const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress,
                               const ImVec2& pos, const ImVec2& size) {
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
            const float headerHeight = 156.0f;
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + headerHeight), tone(*species), GuiCards::ROUNDING, ImDrawFlags_RoundCornersTop);
            ItemIcon::creature(dl, ImVec2(pos.x + 72.0f, pos.y + 66.0f), 44.0f, species->id);
            GuiCards::text(dl, ImVec2(pos.x + 142.0f, pos.y + 18.0f), IM_COL32(255, 255, 255, 255), title(*species, owned).c_str(), 1.5f);
            typeChips(dl, data, *species, ImVec2(pos.x + 142.0f, pos.y + 56.0f));
            char text[64];
            std::snprintf(text, sizeof(text), "Nv. %d  (límite %d)", owned.level, progress.levelCap());
            GuiCards::text(dl, ImVec2(pos.x + 142.0f, pos.y + 86.0f), IM_COL32(255, 255, 255, 230), text, 0.9f);
            ImVec2 chipAt(pos.x + 142.0f, pos.y + 116.0f);
            if (owned.analyzed) {
                std::snprintf(text, sizeof(text), "Potencial %s", EvRules::label(EvRules::rank(species->stats, owned.evCaps)));
                chipAt.x += GuiCards::chip(dl, chipAt, text, rankColor(EvRules::rank(species->stats, owned.evCaps)), 0.85f) + 6.0f;
            } else chipAt.x += GuiCards::chip(dl, chipAt, "Sin analizar", IM_COL32(90, 90, 100, 255), 0.85f) + 6.0f;
            if (owned.shiny) chipAt.x += GuiCards::chip(dl, chipAt, "Variocolor", IM_COL32(210, 170, 40, 255), 0.85f) + 6.0f;
            if (storage.bound(owned)) GuiCards::chip(dl, chipAt, "Inicial", IM_COL32(120, 70, 190, 255), 0.85f);
            if (const PokeballType* ball = data.ball(owned.ballId)) {
                ItemIcon::ball(dl, ImVec2(pos.x + size.x - 28.0f, pos.y + 28.0f), 12.0f, PokeballStyle::color(ball->id));
            }

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f, pos.y + headerHeight + 10.0f));
            ImGui::BeginChild("##detail", ImVec2(size.x - 32.0f, size.y - headerHeight - 74.0f), false);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(species->description.c_str());
            ImGui::PopTextWrapPos();
            if (owned.analyzed) drawStats(species->stats, owned.evs, owned.evCaps);
            else {
                ImGui::Dummy(ImVec2(0.0f, 8.0f));
                muted("Sus estadísticas y su potencial están ocultos. Analízalo con la máquina de investigación para verlos.");
            }
            drawAbilities(data, *species);
            drawGathering(data, *species);
            drawTraining(data, storage, inventory, progress, owned, *species);
            if (!message.empty()) {
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::SUCCESS);
                ImGui::TextWrapped("%s", message.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();

            // Acciones
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f, pos.y + size.y - 54.0f));
            const auto fixIndex = [&]() {
                const int count = static_cast<int>((selection.inTeam ? storage.team() : storage.pc()).size());
                select(selection.inTeam, (std::max)(0, (std::min)(selection.index, count - 1)));
                clearMarks();
            };
            if (selection.inTeam) {
                if (GuiCards::button("##lead", "LÍDER", ImVec2(100.0f, 38.0f), selection.index > 0)) {
                    storage.makeLead(selection.index);
                    select(true, 0);
                    clearMarks();
                }
                ImGui::SameLine();
                if (GuiCards::button("##sendPc", "AL PC", ImVec2(90.0f, 38.0f), !storage.pcFull() && storage.team().size() > 1)) {
                    storage.sendToPc(selection.index);
                    fixIndex();
                }
            } else if (GuiCards::button("##sendTeam", "AL EQUIPO", ImVec2(130.0f, 38.0f), !storage.teamFull())) {
                storage.sendToTeam(selection.index);
                fixIndex();
            }
            ImGui::SameLine();
            const bool canRelease = storage.canRelease(selection.inTeam, selection.index);
            if (GuiCards::button("##release", confirmRelease ? "¿SEGURO? LIBERAR" : "LIBERAR", ImVec2(confirmRelease ? 190.0f : 110.0f, 38.0f), canRelease, IM_COL32(190, 50, 50, 255))) {
                if (confirmRelease) {
                    storage.release(selection.inTeam ? std::vector<int>{ selection.index } : std::vector<int>{},
                                    selection.inTeam ? std::vector<int>{} : std::vector<int>{ selection.index });
                    fixIndex();
                } else confirmRelease = true;
            }
        }
    }

    // ESC o el botón de volver cierran la pantalla (vuelven a 'back').
    inline void render(GameState& state, GameState back, const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));

        // La selección debe seguir apuntando a algo que existe.
        const std::vector<OwnedPokemon>& current = detail::selection.inTeam ? storage.team() : storage.pc();
        if (detail::selection.index >= static_cast<int>(current.size())) detail::select(true, 0);

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float top = detail::MARGIN + 54.0f;
        const float height = screen.y - top - detail::MARGIN - 28.0f;

        if (GuiCards::backButton(ImVec2(detail::MARGIN, detail::MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(detail::MARGIN + 140.0f, detail::MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "POKÉMON", 1.6f);

        detail::drawTeam(data, storage, ImVec2(detail::MARGIN, top), height);

        const float boxX = detail::MARGIN + detail::TEAM_WIDTH + 36.0f;
        const float boxWidth = screen.x - boxX - detail::DETAIL_WIDTH - detail::MARGIN - 28.0f;
        detail::drawBox(data, storage, ImVec2(boxX, top), ImVec2(boxWidth, height));

        detail::drawDetail(data, storage, inventory, progress, ImVec2(screen.x - detail::MARGIN - detail::DETAIL_WIDTH, top), ImVec2(detail::DETAIL_WIDTH, height));

        GuiCards::text(dl, ImVec2(detail::MARGIN, screen.y - detail::MARGIN), GuiStyle::MUTED, "ESC: volver", 0.9f);

        if (GuiInput::backPressed()) state = back;
    }
}
