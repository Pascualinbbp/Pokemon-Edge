#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiPrompts.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../style/icons/teamCircle.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/core/inputDevice.hpp"
#include "../../engine/world/rules/evRules.hpp"
#include "../../engine/world/state/inventory.hpp"
#include "../../engine/world/state/playerProgress.hpp"
#include "../../engine/world/state/pokemonStorage.hpp"
#include "../../models/gameData.hpp"

// Gestión de pokémon: el equipo (6 círculos) a la izquierda, el PC en el centro y la ficha del seleccionado a la derecha.
// Los pokémon se colocan arrastrándolos (ratón) o con el botón de coger y después el de soltar en el destino (mando): sobre otro pokémon
// los intercambia, sobre un hueco o sobre el PC lo mueve. Abajo, los filtros del PC y el control para liberar varios a
// la vez. Sus estadísticas y su potencial salen de la máquina de investigación: hasta entonces están ocultos.
namespace PokemonComponent {
    namespace detail {
        using Place = PokemonStorage::Place;

        inline constexpr float MARGIN = 48.0f;
        inline constexpr float DETAIL_WIDTH = 430.0f;
        inline constexpr float TILE_WIDTH = 96.0f;
        inline constexpr float TILE_HEIGHT = 108.0f;
        inline constexpr float GAP = 12.0f;
        inline constexpr float BAR_HEIGHT = 36.0f;
        inline constexpr float MAX_STAT = 130.0f; // la base más alta que cabe en el gráfico hexagonal
        inline constexpr ImU32 DANGER = IM_COL32(190, 50, 50, 255);
        inline constexpr ImU32 NEUTRAL = IM_COL32(70, 70, 92, 255);

        // Lo que se está moviendo: con el ratón mientras se arrastra; con el mando desde que se pulsa □ hasta soltarlo.
        struct Held {
            bool active = false;
            bool pad = false;
            Place from;
        };
        struct SlotRect {
            Place place;
            ImVec2 a, b;
        };

        inline Place selection;
        inline Held held;
        inline std::vector<SlotRect> slots;      // casillas dibujadas este fotograma (destinos posibles)
        inline ImVec2 teamArea[2], boxArea[2];   // zonas donde soltar un pokémon sin apuntar a una casilla
        inline bool selecting = false;           // modo de selección múltiple para liberar
        inline std::set<int> teamMarks, pcMarks; // pokémon marcados
        inline bool confirmRelease = false;      // hay marcados valiosos: pide confirmación
        inline std::string message;              // resultado del último entrenamiento (evolución...)
        inline int typeFilter = -1;              // índice en data.types (-1 = todos)
        inline int rankFilter = -1;              // potencial mínimo (-1 = todos)
        inline bool shinyFilter = false;

        inline void select(const Place& place) {
            selection = place;
            message.clear();
        }

        inline void clearMarks() {
            teamMarks.clear();
            pcMarks.clear();
            confirmRelease = false;
        }

        inline int markCount() { return static_cast<int>(teamMarks.size() + pcMarks.size()); }

        // Color de fondo de un pokémon: el de su primer tipo.
        inline ImU32 tone(const PokemonSpecies& species) {
            if (!species.types.empty()) return GuiCards::withAlpha(GuiCards::typeColor(species.types.front()), 190);
            return GuiStyle::SURFACE;
        }

        inline void typeChips(ImDrawList* dl, const GameData& data, const PokemonSpecies& species, ImVec2 pos) {
            for (const int id : species.types) {
                if (const PokemonType* type = data.type(id)) pos.x += GuiCards::typeChip(dl, pos, *type) + 6.0f;
            }
        }

        inline const OwnedPokemon* ownedAt(const PokemonStorage& storage, const Place& place) {
            const std::vector<OwnedPokemon>& list = place.inTeam ? storage.team() : storage.pc();
            return place.index >= 0 && place.index < static_cast<int>(list.size()) ? &list[place.index] : nullptr;
        }

        inline std::string title(const PokemonSpecies& species, const OwnedPokemon& owned) { return (owned.shiny ? "* " : "") + species.name; }

        // --- Mover pokémon ---
        inline void drop(PokemonStorage& storage, const Place& to) {
            Place landed = to;
            const Place from = held.from;
            held.active = false;
            if (storage.move(from, to, landed)) {
                select(landed);
                clearMarks();
            } else if (ownedAt(storage, to)) select(to);
        }

        // Entrada de una casilla recién dibujada: la registra como destino, empieza el arrastre (ratón) o la toma (□ del
        // mando) y, en un clic (o ✕ del mando), suelta lo que se lleve. Devuelve true si es un clic normal para seleccionarla o marcarla.
        inline bool slotInput(PokemonStorage& storage, const Place& place, bool filled, bool clicked) {
            slots.push_back({ place, ImGui::GetItemRectMin(), ImGui::GetItemRectMax() });
            if (filled && !selecting && !held.active) {
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) held = { true, false, place };
                else if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false)) held = { true, true, place };
            }
            if (!clicked) return false;
            if (held.active) {
                if (held.pad) drop(storage, place);
                return false;
            }
            return filled;
        }

        // Al soltar el ratón: el pokémon arrastrado va a la casilla bajo el cursor o, si no hay, a la zona (equipo o PC).
        inline void finishDrag(PokemonStorage& storage) {
            if (!held.active || held.pad || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const auto inside = [&](const ImVec2& a, const ImVec2& b) { return mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y; };
            for (const SlotRect& slot : slots) {
                if (inside(slot.a, slot.b)) return drop(storage, slot.place);
            }
            if (inside(teamArea[0], teamArea[1])) return drop(storage, { true, static_cast<int>(storage.team().size()) });
            if (inside(boxArea[0], boxArea[1])) return drop(storage, { false, static_cast<int>(storage.pc().size()) });
            held.active = false;
        }

        // El pokémon que se lleva: pegado al cursor (ratón) o con una marca en su casilla (mando).
        inline void drawHeld(const GameData& data, const PokemonStorage& storage) {
            if (!held.active) return;
            const OwnedPokemon* owned = ownedAt(storage, held.from);
            const PokemonSpecies* species = owned ? data.speciesById(owned->speciesId) : nullptr;
            if (!species) { held.active = false; return; }
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            if (!held.pad) {
                TeamCircle::draw(fg, ImGui::GetIO().MousePos, 34.0f, data, *species, owned->level, owned->shiny, owned->ballId, true, 0, 0.85f);
                return;
            }
            for (const SlotRect& slot : slots) {
                if (!(slot.place == held.from)) continue;
                const ImVec2 c((slot.a.x + slot.b.x) * 0.5f, (slot.a.y + slot.b.y) * 0.5f);
                fg->AddCircle(c, (slot.b.x - slot.a.x) * 0.5f + 5.0f, GuiCards::SELECT, 40, 3.0f);
                GuiCards::centeredText(fg, ImVec2(c.x, slot.a.y - 10.0f), IM_COL32(255, 255, 255, 255), "Elige destino", 0.8f);
            }
        }

        // --- Equipo ---
        inline void drawTeam(const GameData& data, PokemonStorage& storage, const ImVec2& pos, float height) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            char header[48];
            std::snprintf(header, sizeof(header), "EQUIPO  %d / %d", static_cast<int>(storage.team().size()), PokemonStorage::TEAM_SIZE);
            GuiCards::text(dl, pos, IM_COL32(255, 255, 255, 255), header, 1.1f);

            const float top = pos.y + 40.0f;
            const float radius = (std::min)(46.0f, (height - 40.0f - 5.0f * GAP) / 12.0f);
            const float x = pos.x + radius;
            teamArea[0] = ImVec2(pos.x - 6.0f, top - 6.0f);
            teamArea[1] = ImVec2(pos.x + radius * 2.0f + 6.0f, top + 6.0f * radius * 2.0f + 5.0f * GAP + 6.0f);

            for (int i = 0; i < PokemonStorage::TEAM_SIZE; ++i) {
                const Place place{ true, i };
                const OwnedPokemon* owned = ownedAt(storage, place);
                const PokemonSpecies* species = owned ? data.speciesById(owned->speciesId) : nullptr;
                const ImVec2 c(x, top + radius + static_cast<float>(i) * (radius * 2.0f + GAP));
                ImGui::SetCursorScreenPos(ImVec2(c.x - radius, c.y - radius));
                ImGui::PushID(i);
                const bool clicked = ImGui::InvisibleButton("##slot", ImVec2(radius * 2.0f, radius * 2.0f));
                ImGui::PopID();
                const bool hovered = ImGui::IsItemHovered() || ImGui::IsItemFocused();
                const bool picked = slotInput(storage, place, species != nullptr, clicked);

                const bool target = held.active && hovered && !(held.from == place);
                if (species) {
                    TeamCircle::draw(dl, c, radius, data, *species, owned->level, owned->shiny, owned->ballId, i == storage.activeIndex(), i + 1,
                                     held.active && held.from == place ? 0.35f : 1.0f);
                } else TeamCircle::empty(dl, c, radius, target);
                if (target) dl->AddCircle(c, radius + 4.0f, GuiCards::SELECT, 40, 2.5f);
                if (selection == place) dl->AddCircle(c, radius + 5.0f, GuiCards::SELECT, 40, 3.0f);
                if (selecting && species && !storage.bound(*owned)) {
                    const ImVec2 corner(c.x + radius * 0.8f, c.y - radius * 0.8f);
                    dl->AddRectFilled(ImVec2(corner.x - 10.0f, corner.y - 10.0f), ImVec2(corner.x + 10.0f, corner.y + 10.0f), IM_COL32(0, 0, 0, 170), 4.0f);
                    dl->AddRect(ImVec2(corner.x - 10.0f, corner.y - 10.0f), ImVec2(corner.x + 10.0f, corner.y + 10.0f), IM_COL32(255, 255, 255, 230), 4.0f, 0, 1.5f);
                    if (teamMarks.count(i)) dl->AddRectFilled(ImVec2(corner.x - 6.0f, corner.y - 6.0f), ImVec2(corner.x + 6.0f, corner.y + 6.0f), IM_COL32(255, 120, 90, 255), 2.0f);
                }
                if (!picked) continue;
                if (selecting) {
                    if (!storage.bound(*owned) && !teamMarks.erase(i)) teamMarks.insert(i);
                    confirmRelease = false;
                } else select(place);
            }
        }

        // --- PC con filtros ---
        inline bool passesFilter(const GameData& data, const OwnedPokemon& owned) {
            const PokemonSpecies* species = data.speciesById(owned.speciesId);
            if (!species) return false;
            if (typeFilter >= 0 && typeFilter < static_cast<int>(data.types.size()) &&
                std::find(species->types.begin(), species->types.end(), data.types[typeFilter].id) == species->types.end()) return false;
            if (shinyFilter && !owned.shiny) return false;
            if (rankFilter >= 0 && !(owned.analyzed && EvRules::rank(species->stats, owned.evCaps) >= EvRules::RANKS[rankFilter])) return false;
            return true;
        }

        inline void drawBox(const GameData& data, PokemonStorage& storage, const ImVec2& pos, const ImVec2& size) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            char header[48];
            std::snprintf(header, sizeof(header), "PC  %d / %d", static_cast<int>(storage.pc().size()), PokemonStorage::PC_CAPACITY);
            GuiCards::text(dl, pos, IM_COL32(255, 255, 255, 255), header, 1.1f);

            boxArea[0] = ImVec2(pos.x, pos.y + 34.0f);
            boxArea[1] = ImVec2(pos.x + size.x, pos.y + size.y);
            ImGui::SetCursorScreenPos(boxArea[0]);
            ImGui::BeginChild("##box", ImVec2(size.x, size.y - 34.0f), false);
            const int columns = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x + GAP) / (TILE_WIDTH + GAP)));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(GAP, GAP));
            char badge[16];
            int shown = 0;
            const auto cell = [&]() { if (shown++ % columns != 0) ImGui::SameLine(); };
            for (int i = 0; i < static_cast<int>(storage.pc().size()); ++i) {
                const OwnedPokemon& owned = storage.pc()[i];
                const PokemonSpecies* species = data.speciesById(owned.speciesId);
                if (!species || !passesFilter(data, owned)) continue;
                cell();
                const Place place{ false, i };
                ImGui::PushID(i);
                std::snprintf(badge, sizeof(badge), "Nv %d", owned.level);
                const bool clicked = GuiCards::tile("##pc", ImVec2(TILE_WIDTH, TILE_HEIGHT), selection == place,
                    tone(*species), [&](ImDrawList* d, const ImVec2& c, float r) { ItemIcon::creature(d, c, r, species->id, held.active && held.from == place ? 0.35f : 1.0f); },
                    title(*species, owned).c_str(), badge);
                ImGui::PopID();
                const bool picked = slotInput(storage, place, true, clicked);
                if (held.active && !(held.from == place) && (ImGui::IsItemHovered() || ImGui::IsItemFocused())) {
                    dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), GuiCards::SELECT, 10.0f, 0, 2.5f);
                }
                if (selecting && !storage.bound(owned)) {
                    const ImVec2 a(ImGui::GetItemRectMax().x - 24.0f, ImGui::GetItemRectMin().y + 6.0f);
                    dl->AddRectFilled(a, ImVec2(a.x + 18.0f, a.y + 18.0f), IM_COL32(0, 0, 0, 160), 4.0f);
                    dl->AddRect(a, ImVec2(a.x + 18.0f, a.y + 18.0f), IM_COL32(255, 255, 255, 230), 4.0f, 0, 1.5f);
                    if (pcMarks.count(i)) dl->AddRectFilled(ImVec2(a.x + 4.0f, a.y + 4.0f), ImVec2(a.x + 14.0f, a.y + 14.0f), IM_COL32(255, 120, 90, 255), 2.0f);
                }
                if (!picked) continue;
                if (selecting) {
                    if (!storage.bound(owned) && !pcMarks.erase(i)) pcMarks.insert(i);
                    confirmRelease = false;
                } else select(place);
            }

            // Hueco final: destino para mandar un pokémon del equipo al PC.
            if (!storage.pcFull()) {
                cell();
                const Place place{ false, static_cast<int>(storage.pc().size()) };
                ImGui::PushID(100000);
                const bool clicked = ImGui::InvisibleButton("##append", ImVec2(TILE_WIDTH, TILE_HEIGHT));
                ImGui::PopID();
                slotInput(storage, place, false, clicked);
                const bool target = held.active && (ImGui::IsItemHovered() || ImGui::IsItemFocused());
                const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
                dl->AddRect(a, b, IM_COL32(255, 255, 255, target ? 200 : 45), 10.0f, 0, target ? 3.0f : 1.5f);
                GuiCards::centeredText(dl, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), GuiStyle::MUTED, "+", 1.8f);
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        // --- Barra inferior: filtros del PC y liberar ---
        inline std::string capital(std::string name) {
            if (!name.empty() && static_cast<unsigned char>(name[0]) < 128) name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            return name;
        }

        inline float barButton(const char* id, const std::string& label, float x, float y, bool enabled, ImU32 fill, bool& pressed) {
            const float w = GuiCards::textWidth(label.c_str(), 0.95f) + 32.0f;
            ImGui::SetCursorScreenPos(ImVec2(x, y));
            pressed = GuiCards::button(id, label.c_str(), ImVec2(w, BAR_HEIGHT - 6.0f), enabled, fill);
            return w + 8.0f;
        }

        inline void drawBar(const GameData& data, PokemonStorage& storage, const ImVec2& pos, float width) {
            bool pressed = false;
            float x = pos.x;
            x += barButton("##fType", "TIPO: " + (typeFilter < 0 ? std::string("TODOS") : capital(data.types[typeFilter].name)), x, pos.y, true, NEUTRAL, pressed);
            if (pressed) typeFilter = typeFilter + 1 < static_cast<int>(data.types.size()) ? typeFilter + 1 : -1;
            x += barButton("##fRank", std::string("POTENCIAL: ") + (rankFilter < 0 ? "TODOS" : std::string(">= ") + EvRules::label(EvRules::RANKS[rankFilter])), x, pos.y, true, NEUTRAL, pressed);
            if (pressed) rankFilter = rankFilter + 1 < static_cast<int>(std::size(EvRules::RANKS)) ? rankFilter + 1 : -1;
            x += barButton("##fShiny", shinyFilter ? "VARIOCOLOR: SÍ" : "VARIOCOLOR", x, pos.y, true, shinyFilter ? IM_COL32(190, 150, 30, 255) : NEUTRAL, pressed);
            if (pressed) shinyFilter = !shinyFilter;

            // Liberar: a la derecha. Solo pide confirmación si entre los marcados hay variocolor o de mucho potencial.
            float right = pos.x + width;
            const auto place = [&](const std::string& label) { return right - (GuiCards::textWidth(label.c_str(), 0.95f) + 32.0f); };
            const std::string selectLabel = selecting ? "CANCELAR" : "LIBERAR VARIOS";
            right = place(selectLabel);
            barButton("##selectMode", selectLabel, right, pos.y, true, NEUTRAL, pressed);
            if (pressed) {
                selecting = !selecting;
                clearMarks();
            }
            if (!selecting) return;

            right -= 8.0f;
            char text[48];
            std::snprintf(text, sizeof(text), confirmRelease ? "¿SEGURO? LIBERAR (%d)" : "LIBERAR (%d)", markCount());
            right = place(text);
            barButton("##release", text, right, pos.y, markCount() > 0, DANGER, pressed);
            if (pressed) {
                bool risky = false;
                for (const int i : teamMarks) if (const OwnedPokemon* owned = ownedAt(storage, { true, i })) risky |= storage.valuable(*owned);
                for (const int i : pcMarks) if (const OwnedPokemon* owned = ownedAt(storage, { false, i })) risky |= storage.valuable(*owned);
                if (risky && !confirmRelease) confirmRelease = true;
                else {
                    storage.release(std::vector<int>(teamMarks.begin(), teamMarks.end()), std::vector<int>(pcMarks.begin(), pcMarks.end()));
                    clearMarks();
                    select({ true, 0 });
                    selecting = false; // terminada la liberación se sale del modo
                }
            }

            right -= 8.0f;
            std::vector<int> visible;
            for (int i = 0; i < static_cast<int>(storage.pc().size()); ++i) {
                if (passesFilter(data, storage.pc()[i]) && !storage.bound(storage.pc()[i])) visible.push_back(i);
            }
            const std::string allLabel = "MARCAR VISIBLES";
            right = place(allLabel);
            barButton("##markAll", allLabel, right, pos.y, !visible.empty(), NEUTRAL, pressed);
            if (pressed) {
                const bool all = std::all_of(visible.begin(), visible.end(), [](int i) { return pcMarks.count(i) > 0; });
                for (const int i : visible) { if (all) pcMarks.erase(i); else pcMarks.insert(i); }
                confirmRelease = false;
            }
        }

        // --- Ficha ---
        // Gráfico hexagonal: estadísticas base (azul) y, encima, su crecimiento por EVs (naranja). En cada vértice: nombre y
        // base, EV actual / máximo con el que salió y un "+" que gasta una vitamina con las disponibles / necesarias para llegar al máximo.
        inline void drawStats(const GameData& data, PokemonStorage& storage, Inventory& inventory, const OwnedPokemon& owned,
                              const PokemonSpecies& species, const ImVec2& area, float avail) {
            constexpr int ORDER[6] = { 0, 1, 3, 5, 4, 2 }; // vértices de arriba en sentido horario (índices de BaseStats)
            constexpr float PI = 3.14159265f;
            constexpr float BLOCK_W = 96.0f, BLOCK_H = 50.0f;
            float base[6], grown[6];
            for (int i = 0; i < 6; ++i) {
                base[i] = static_cast<float>(species.stats.at(ORDER[i])) / MAX_STAT;
                grown[i] = static_cast<float>(species.stats.at(ORDER[i]) + owned.evs[ORDER[i]]) / MAX_STAT;
            }
            const float radius = (std::clamp)((avail - 2.0f * BLOCK_H - 16.0f) * 0.5f, 34.0f, 86.0f);
            const ImVec2 center(area.x + (DETAIL_WIDTH - 32.0f) * 0.5f, area.y + BLOCK_H + 8.0f + radius);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            GuiCards::radar(dl, center, radius, base, GuiStyle::ACCENT, &grown, IM_COL32(255, 160, 40, 255));

            const TrainingItem* vitamin = data.trainingWith(TrainingItem::Effect::EV);
            const Item* vitaminItem = vitamin ? data.itemOf(ItemCategory::TRAINING, vitamin->id) : nullptr;
            const int count = vitaminItem ? inventory.count(vitaminItem->id) : 0;
            char line[48];
            for (int i = 0; i < 6; ++i) {
                const float angle = -PI * 0.5f + static_cast<float>(i) * PI / 3.0f;
                const float cx = std::cos(angle), cy = std::sin(angle);
                const int stat = ORDER[i];
                const ImVec2 anchor(center.x + cx * (radius + 10.0f), center.y + cy * (radius + 10.0f));
                const float x = anchor.x + (cx > 0.3f ? 0.0f : cx < -0.3f ? -BLOCK_W : -BLOCK_W * 0.5f);
                const float y = anchor.y + (cy < -0.3f ? -BLOCK_H : cy > 0.3f ? 0.0f : -BLOCK_H * 0.5f);
                const float lineH = ImGui::GetFontSize() * 0.85f + 1.0f;

                std::snprintf(line, sizeof(line), "%s %d", BaseStats::name(stat), species.stats.at(stat));
                GuiCards::text(dl, ImVec2(x, y), IM_COL32(255, 255, 255, 230), line, 0.85f);
                std::snprintf(line, sizeof(line), "%d/%d", owned.evs[stat], owned.evCaps[stat]);
                GuiCards::text(dl, ImVec2(x, y + lineH), IM_COL32(255, 170, 60, 255), line, 0.85f);

                const int missing = owned.evCaps[stat] - owned.evs[stat];
                const int needed = vitamin ? (missing + vitamin->amount - 1) / vitamin->amount : 0;
                const bool usable = missing > 0 && count > 0;
                const ImVec2 button(x + 11.0f, y + lineH * 2.0f + 12.0f);
                ImGui::SetCursorScreenPos(ImVec2(button.x - 11.0f, button.y - 11.0f));
                ImGui::PushID(stat);
                const bool pressed = ImGui::InvisibleButton("##ev", ImVec2(22.0f, 22.0f)) && usable;
                ImGui::PopID();
                const bool hot = usable && (ImGui::IsItemHovered() || ImGui::IsItemFocused());
                dl->AddCircleFilled(button, 11.0f, usable ? (hot ? IM_COL32(255, 190, 80, 255) : IM_COL32(230, 140, 40, 255)) : IM_COL32(70, 70, 80, 200), 20);
                dl->AddLine(ImVec2(button.x - 5.0f, button.y), ImVec2(button.x + 5.0f, button.y), IM_COL32(255, 255, 255, usable ? 255 : 120), 2.0f);
                dl->AddLine(ImVec2(button.x, button.y - 5.0f), ImVec2(button.x, button.y + 5.0f), IM_COL32(255, 255, 255, usable ? 255 : 120), 2.0f);
                if (missing > 0) std::snprintf(line, sizeof(line), "%d/%d", count, needed);
                else std::snprintf(line, sizeof(line), "MAX");
                GuiCards::text(dl, ImVec2(button.x + 16.0f, button.y - ImGui::GetFontSize() * 0.4f), usable ? IM_COL32(255, 255, 255, 255) : GuiStyle::MUTED, line, 0.85f);
                if (pressed && storage.train(selection.inTeam, selection.index, stat, vitamin->amount)) inventory.add(vitaminItem->id, -1);
            }
        }

        // Habilidad activa o pasiva: ficha con su nombre y su descripción al lado, en una línea.
        inline void abilityLine(ImDrawList* dl, const Ability* ability, const char* kind, ImU32 color, bool describe) {
            if (!ability) return;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float w = GuiCards::chip(dl, at, (std::string(kind) + ": " + ability->name).c_str(), color, 0.85f);
            if (describe) {
                ImGui::SetCursorScreenPos(ImVec2(at.x + w + 8.0f, at.y + 2.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
                ImGui::PushTextWrapPos(at.x + DETAIL_WIDTH - 32.0f - ImGui::GetWindowPos().x);
                ImGui::TextUnformatted(ability->description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
            ImGui::SetCursorScreenPos(ImVec2(at.x, at.y + ImGui::GetFontSize() * (describe ? 2.4f : 1.5f)));
        }

        inline void drawDetail(const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress,
                               const ImVec2& pos, const ImVec2& size) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y));

            const OwnedPokemon* found = ownedAt(storage, selection);
            const PokemonSpecies* species = found ? data.speciesById(found->speciesId) : nullptr;
            if (!species) {
                GuiCards::centeredText(dl, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), GuiStyle::MUTED, "Selecciona un pokémon", 0.95f);
                return;
            }
            const OwnedPokemon owned = *found;

            // Cabecera con el color de su tipo.
            const float headerHeight = 142.0f;
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + headerHeight), tone(*species), GuiCards::ROUNDING, ImDrawFlags_RoundCornersTop);
            ItemIcon::creature(dl, ImVec2(pos.x + 56.0f, pos.y + 54.0f), 34.0f, species->id);
            GuiCards::text(dl, ImVec2(pos.x + 108.0f, pos.y + 12.0f), IM_COL32(255, 255, 255, 255), title(*species, owned).c_str(), 1.4f);
            typeChips(dl, data, *species, ImVec2(pos.x + 108.0f, pos.y + 42.0f));
            char text[64];
            std::snprintf(text, sizeof(text), "Nv. %d", owned.level);
            GuiCards::text(dl, ImVec2(pos.x + 108.0f, pos.y + 72.0f), IM_COL32(255, 255, 255, 230), text, 1.0f);
            ImVec2 chipAt(pos.x + 108.0f + GuiCards::textWidth(text) + 12.0f, pos.y + 72.0f);
            if (owned.analyzed) {
                const EvRules::Rank rank = EvRules::rank(species->stats, owned.evCaps);
                std::snprintf(text, sizeof(text), "Potencial %s", EvRules::label(rank));
                chipAt.x += GuiCards::chip(dl, chipAt, text, GuiCards::rankColor(rank), 0.8f) + 6.0f;
            } else chipAt.x += GuiCards::chip(dl, chipAt, "Sin analizar", IM_COL32(90, 90, 100, 255), 0.8f) + 6.0f;
            if (owned.shiny) chipAt.x += GuiCards::chip(dl, chipAt, "Variocolor", IM_COL32(210, 170, 40, 255), 0.8f) + 6.0f;
            if (storage.bound(owned)) GuiCards::chip(dl, chipAt, "Inicial", IM_COL32(120, 70, 190, 255), 0.8f);
            if (const PokeballType* ball = data.ball(owned.ballId)) ItemIcon::ball(dl, ImVec2(pos.x + size.x - 26.0f, pos.y + 26.0f), 12.0f, PokeballStyle::color(ball->id));

            // Caramelo: sube un nivel (sin pasar del límite del jugador).
            if (const TrainingItem* candy = data.trainingWith(TrainingItem::Effect::LEVEL)) if (const Item* item = data.itemOf(ItemCategory::TRAINING, candy->id)) {
                const int count = inventory.count(item->id);
                std::snprintf(text, sizeof(text), "NIVEL +1  x%d", count);
                ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - 150.0f, pos.y + 44.0f));
                if (GuiCards::button("##candy", text, ImVec2(134.0f, 26.0f), count > 0 && owned.level < progress.levelCap(), IM_COL32(40, 150, 120, 255))) {
                    inventory.add(item->id, -1);
                    const std::string before = species->name;
                    const PokemonStorage::LevelResult result = storage.levelUp(selection.inTeam, selection.index, progress.levelCap());
                    const PokemonSpecies* after = ownedAt(storage, selection) ? data.speciesById(ownedAt(storage, selection)->speciesId) : nullptr;
                    message = result == PokemonStorage::LevelResult::EVOLVED && after ? "¡" + before + " ha evolucionado a " + after->name + "!" : "";
                }
            }

            // Modo del equipo entero (solo en la ficha de los del equipo): recolección, captura o combate.
            if (selection.inTeam) {
                std::snprintf(text, sizeof(text), "MODO: %s", CompanionRules::label(storage.mode));
                ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f, pos.y + 106.0f));
                if (GuiCards::button("##mode", text, ImVec2(200.0f, 28.0f), true, IM_COL32(70, 90, 150, 255))) storage.cycleMode();
            }

            const float left = pos.x + 16.0f, width = size.x - 32.0f;
            float y = pos.y + headerHeight + 10.0f;
            const bool compact = size.y < 640.0f;
            if (!compact) {
                ImGui::SetCursorScreenPos(ImVec2(left, y));
                ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
                ImGui::PushTextWrapPos(left + width - ImGui::GetWindowPos().x);
                ImGui::TextUnformatted(species->description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                y = ImGui::GetCursorScreenPos().y + 6.0f;
            }

            // Abajo: habilidades y habilidad del mundo (alturas fijas); el resto es para el gráfico.
            const float bottomHeight = compact ? 86.0f : 130.0f;
            const float statsAvail = pos.y + size.y - bottomHeight - y - 8.0f;
            if (owned.analyzed) drawStats(data, storage, inventory, owned, *species, ImVec2(left, y), statsAvail);
            else {
                ImGui::SetCursorScreenPos(ImVec2(left, y + 8.0f));
                ImGui::PushTextWrapPos(left + width - ImGui::GetWindowPos().x);
                GuiCards::muted("Sus estadísticas y su potencial están ocultos. Analízalo con la máquina de investigación.");
                ImGui::PopTextWrapPos();
            }

            ImGui::SetCursorScreenPos(ImVec2(left, pos.y + size.y - bottomHeight));
            abilityLine(dl, data.ability(species->abilityId), "Activa", IM_COL32(190, 80, 60, 255), !compact);
            if (species->passiveId >= 0) abilityLine(dl, data.ability(species->passiveId), "Pasiva", IM_COL32(130, 80, 190, 255), !compact);

            const Skill* skill = data.skill(species->skillId);
            std::snprintf(text, sizeof(text), "%s nivel %d", skill ? skill->name.c_str() : "?", species->skillLevel);
            std::string works;
            for (const ResourceNodeType& node : data.nodes) {
                if (node.skillId == species->skillId && node.level <= species->skillLevel) works += (works.empty() ? "" : ", ") + node.name;
            }
            ImGui::PushTextWrapPos(left + width - ImGui::GetWindowPos().x);
            ImGui::TextUnformatted((std::string(text) + (works.empty() ? "" : "  ·  " + works)).c_str());
            ImGui::PopTextWrapPos();
            if (!message.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::SUCCESS);
                ImGui::TextUnformatted(message.c_str());
                ImGui::PopStyleColor();
            }
        }
    }

    // ESC o el botón de volver cierran la pantalla (vuelven a 'back'); con un pokémon en la mano, primero lo suelta.
    inline void render(GameState& state, GameState back, const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress, InputDevice device) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));

        // La selección debe seguir apuntando a algo que existe.
        if (!detail::ownedAt(storage, detail::selection)) detail::select({ true, 0 });

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float top = detail::MARGIN + 54.0f;
        const float barY = screen.y - detail::MARGIN - detail::BAR_HEIGHT + 4.0f;
        const float height = barY - top - 10.0f;

        if (GuiCards::backButton(ImVec2(detail::MARGIN, detail::MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(detail::MARGIN + 140.0f, detail::MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "POKÉMON", 1.6f);

        detail::slots.clear();
        const float teamWidth = 2.0f * (std::min)(46.0f, (height - 40.0f - 5.0f * detail::GAP) / 12.0f);
        detail::drawTeam(data, storage, ImVec2(detail::MARGIN, top), height);

        const float boxX = detail::MARGIN + teamWidth + 36.0f;
        const float boxWidth = screen.x - boxX - detail::DETAIL_WIDTH - detail::MARGIN - 28.0f;
        detail::drawBox(data, storage, ImVec2(boxX, top), ImVec2(boxWidth, height));
        detail::drawBar(data, storage, ImVec2(boxX, barY), boxWidth);

        detail::drawDetail(data, storage, inventory, progress, ImVec2(screen.x - detail::MARGIN - detail::DETAIL_WIDTH, top), ImVec2(detail::DETAIL_WIDTH, height));

        const ImVec2 hint(detail::MARGIN + 300.0f, detail::MARGIN - 4.0f);
        if (!isGamepad(device)) GuiCards::text(dl, ImVec2(hint.x, hint.y + 4.0f), GuiStyle::MUTED, "Arrastra un pokémon para colocarlo o intercambiarlo", 0.9f);
        else {
            const float w = GuiPrompts::draw(dl, hint, GuiPrompts::Action::INTERACT, "Coger", device);
            GuiPrompts::draw(dl, ImVec2(hint.x + w + 24.0f, hint.y), GuiPrompts::Action::JUMP, "Soltar en el destino", device);
        }
        detail::drawHeld(data, storage);
        detail::finishDrag(storage);

        if (GuiInput::backPressed()) {
            if (detail::held.active) detail::held.active = false;
            else state = back;
        }
    }
}
