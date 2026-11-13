#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
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
#include "moveEditor.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/core/inputDevice.hpp"
#include "../../engine/world/rules/ivRules.hpp"
#include "../../engine/world/rules/statRules.hpp"
#include "statEditor.hpp"
#include "../../engine/world/state/inventory.hpp"
#include "../../engine/world/state/itemUse.hpp"
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

        inline bool manage = false;              // true en el PC de la máquina del inicio: solo ahí se mueven y liberan pokémon; en el menú rápido solo se ven
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
        inline bool editingMoves = false;        // editor de movimientos abierto para el pokémon seleccionado
        inline bool editingStats = false;        // editor de EVs y naturaleza abierto para el pokémon seleccionado (solo en el PC)
        inline int tab = 0;                      // ficha del menú rápido: 0 presentación, 1 entrenamiento (EVs y naturaleza), 2 movimientos
        inline float wrapWidth = DETAIL_WIDTH - 32.0f; // ancho de línea de las habilidades
        inline float heldNotice = 0.0f;                // hasta cuándo se ve el aviso de que no hay objetos equipables

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
            if ((manage || place.inTeam) && filled && !selecting && !held.active) { // en el menú rápido solo se reordena el equipo
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
                if (inside(slot.a, slot.b)) {
                    if (!manage && !ownedAt(storage, slot.place)) break; // menú rápido: solo se intercambia con otro pokémon del equipo
                    return drop(storage, slot.place);
                }
            }
            if (!manage) { held.active = false; return; }
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
                TeamCircle::draw(fg, ImGui::GetIO().MousePos, 34.0f, data, *species, owned->level, owned->shiny, owned->ballId, true, 0, 0.85f, storage.fainted(*owned));
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
                                     held.active && held.from == place ? 0.35f : 1.0f, storage.fainted(*owned));
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
            if (rankFilter >= 0 && !(owned.analyzed && IvRules::rank(owned.ivCaps) >= IvRules::RANKS[rankFilter])) return false;
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
                    tone(*species), [&](ImDrawList* d, const ImVec2& c, float r) { ItemIcon::creature(d, c, r, species->id, held.active && held.from == place ? 0.35f : 1.0f, 0.0f, storage.fainted(owned)); },
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
            if (manage && !storage.pcFull()) {
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
            x += barButton("##fRank", std::string("POTENCIAL: ") + (rankFilter < 0 ? "TODOS" : std::string(">= ") + IvRules::label(IvRules::RANKS[rankFilter])), x, pos.y, true, NEUTRAL, pressed);
            if (pressed) rankFilter = rankFilter + 1 < static_cast<int>(std::size(IvRules::RANKS)) ? rankFilter + 1 : -1;
            x += barButton("##fShiny", shinyFilter ? "VARIOCOLOR: SÍ" : "VARIOCOLOR", x, pos.y, true, shinyFilter ? IM_COL32(190, 150, 30, 255) : NEUTRAL, pressed);
            if (pressed) shinyFilter = !shinyFilter;

            if (!manage) return;
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
        // Gráfico hexagonal: estadísticas base (azul) y, encima, lo que suman sus IV y EVs (naranja). En cada vértice: nombre, base y
        // estadística real (con nivel, IV, EVs y naturaleza), y el IV actual / tope con el que salió. Las vitaminas y chapas se usan desde la mochila.
        inline void drawStats(const GameData& data, PokemonStorage& storage, Inventory& inventory, const OwnedPokemon& owned,
                              const PokemonSpecies& species, const ImVec2& area, float avail, float width = DETAIL_WIDTH - 32.0f) {
            constexpr int ORDER[6] = { 0, 1, 3, 5, 4, 2 }; // vértices de arriba en sentido horario (índices de BaseStats)
            constexpr float PI = 3.14159265f;
            constexpr float BLOCK_W = 96.0f, BLOCK_H = 50.0f;
            float base[6], grown[6];
            for (int i = 0; i < 6; ++i) {
                base[i] = static_cast<float>(species.stats.at(ORDER[i])) / MAX_STAT;
                grown[i] = static_cast<float>(species.stats.at(ORDER[i])) / MAX_STAT + static_cast<float>(owned.ivs[ORDER[i]] + owned.evs[ORDER[i]] / 4) / (2.0f * MAX_STAT);
            }
            const float radius = (std::clamp)((avail - 2.0f * BLOCK_H - 16.0f) * 0.5f, 34.0f, 86.0f);
            const ImVec2 center(area.x + width * 0.5f, area.y + BLOCK_H + 8.0f + radius);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            GuiCards::radar(dl, center, radius, base, GuiStyle::ACCENT, &grown, IM_COL32(255, 160, 40, 255));

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
                std::snprintf(line, sizeof(line), "%d  IV %d/%d", storage.statOf(owned, stat), owned.ivs[stat], owned.ivCaps[stat]);
                GuiCards::text(dl, ImVec2(x, y + lineH), IM_COL32(255, 170, 60, 255), line, 0.85f);

            }
        }

        // Habilidad activa o pasiva: ficha con su nombre y su descripción al lado, en una línea.
        inline void abilityLine(ImDrawList* dl, const Ability* ability, const char* kind, ImU32 color, bool describe) {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float w = GuiCards::chip(dl, at, (std::string(kind) + ": " + (ability ? ability->name : std::string("ninguna"))).c_str(), color, 0.85f);
            if (describe && ability) {
                ImGui::SetCursorScreenPos(ImVec2(at.x + w + 8.0f, at.y + 2.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
                ImGui::PushTextWrapPos(at.x + wrapWidth - ImGui::GetWindowPos().x);
                ImGui::TextUnformatted(ability->description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
            ImGui::SetCursorScreenPos(ImVec2(at.x, at.y + ImGui::GetFontSize() * (describe ? 2.4f : 1.5f)));
        }

        // Habilidad del mundo (la que usa fuera del combate: talar, picar, regar, pescar...): su nivel y qué puede trabajar.
        inline void worldSkillLine(ImDrawList* dl, const GameData& data, const PokemonSpecies& species, bool describe) {
            const Skill* skill = data.skill(species.skillId);
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const std::string label = std::string("Mundo: ") + (skill ? skill->name : std::string("?")) + " nivel " + std::to_string(species.skillLevel);
            const float w = GuiCards::chip(dl, at, label.c_str(), IM_COL32(60, 140, 90, 255), 0.85f);
            if (describe && skill) {
                std::string works;
                for (const ResourceNodeType& node : data.nodes) {
                    if (node.skillId == species.skillId && node.level <= species.skillLevel) works += (works.empty() ? "" : ", ") + node.name;
                }
                ImGui::SetCursorScreenPos(ImVec2(at.x + w + 8.0f, at.y + 2.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
                ImGui::PushTextWrapPos(at.x + wrapWidth - ImGui::GetWindowPos().x);
                ImGui::TextUnformatted((skill->description + (works.empty() ? "" : " Trabaja: " + works + ".")).c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
            ImGui::SetCursorScreenPos(ImVec2(at.x, at.y + ImGui::GetFontSize() * (describe ? 2.9f : 1.5f)));
        }

        inline void drawDetail(const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress,
                               const ImVec2& pos, const ImVec2& size, bool embedded = false) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (!embedded) GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y)); // embedded: dentro de la pestaña de la ficha (sin marco, botones ni textos de abajo)

            const OwnedPokemon* found = ownedAt(storage, selection);
            const PokemonSpecies* species = found ? data.speciesById(found->speciesId) : nullptr;
            if (!species) {
                GuiCards::centeredText(dl, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), GuiStyle::MUTED, "Selecciona un pokémon", 0.95f);
                return;
            }
            const OwnedPokemon owned = *found;

            // Cabecera con el color de su tipo.
            const float headerHeight = 172.0f;
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + headerHeight), tone(*species), GuiCards::ROUNDING, ImDrawFlags_RoundCornersTop);
            ItemIcon::creature(dl, ImVec2(pos.x + 56.0f, pos.y + 54.0f), 34.0f, species->id);
            GuiCards::text(dl, ImVec2(pos.x + 108.0f, pos.y + 12.0f), IM_COL32(255, 255, 255, 255), title(*species, owned).c_str(), 1.4f);
            typeChips(dl, data, *species, ImVec2(pos.x + 108.0f, pos.y + 42.0f));
            char text[64];
            std::snprintf(text, sizeof(text), "Nv. %d", owned.level);
            GuiCards::text(dl, ImVec2(pos.x + 108.0f, pos.y + 72.0f), IM_COL32(255, 255, 255, 230), text, 1.0f);
            ImVec2 chipAt(pos.x + 108.0f + GuiCards::textWidth(text) + 12.0f, pos.y + 72.0f);
            if (owned.analyzed) {
                const IvRules::Rank rank = IvRules::rank(owned.ivCaps);
                std::snprintf(text, sizeof(text), "Potencial %s", IvRules::label(rank));
                chipAt.x += GuiCards::chip(dl, chipAt, text, GuiCards::rankColor(rank), 0.8f) + 6.0f;
            } else chipAt.x += GuiCards::chip(dl, chipAt, "Sin analizar", IM_COL32(90, 90, 100, 255), 0.8f) + 6.0f;
            if (owned.shiny) chipAt.x += GuiCards::chip(dl, chipAt, "Variocolor", IM_COL32(210, 170, 40, 255), 0.8f) + 6.0f;
            if (storage.bound(owned)) GuiCards::chip(dl, chipAt, "Inicial", IM_COL32(120, 70, 190, 255), 0.8f);
            if (const PokeballType* ball = data.ball(owned.ballId)) ItemIcon::ball(dl, ImVec2(pos.x + size.x - 26.0f, pos.y + 26.0f), 12.0f, PokeballStyle::look(ball->id));

            // PS (barra). Los objetos se usan solo desde la mochila.
            {
                const int maxHp = storage.maxHp(owned), hp = storage.hpOf(owned);
                const ImVec2 bar(pos.x + 108.0f, pos.y + 92.0f);
                const float barW = size.x - 108.0f - 16.0f;
                dl->AddRectFilled(bar, ImVec2(bar.x + barW, bar.y + 16.0f), IM_COL32(0, 0, 0, 130), 8.0f);
                const float f = storage.hpFraction(owned);
                if (f > 0.0f) dl->AddRectFilled(bar, ImVec2(bar.x + barW * f, bar.y + 16.0f), f > 0.5f ? IM_COL32(90, 210, 110, 255) : f > 0.2f ? IM_COL32(235, 200, 60, 255) : IM_COL32(230, 70, 70, 255), 8.0f);
                std::snprintf(text, sizeof(text), hp > 0 ? "PS %d/%d" : "PS 0/%d  (debilitado)", hp > 0 ? hp : maxHp, maxHp);
                GuiCards::text(dl, ImVec2(bar.x + 8.0f, bar.y + 1.0f), IM_COL32(255, 255, 255, 245), text, 0.75f);

                // Objeto equipable: se da desde la mochila; aquí se ve y se puede quitar (vuelve a la mochila).
                const ImVec2 slot(pos.x + 16.0f, pos.y + headerHeight - 58.0f);
                if (const HeldItem* held = storage.heldOf(owned)) {
                    ItemIcon::held(dl, ImVec2(slot.x + 12.0f, slot.y + 13.0f), 11.0f, *held);
                    GuiCards::text(dl, ImVec2(slot.x + 30.0f, slot.y + 5.0f), IM_COL32(255, 255, 255, 235), held->name.c_str(), 0.85f);
                    const float nameWidth = ImGui::CalcTextSize(held->name.c_str()).x * 0.85f;
                    ImGui::SetCursorScreenPos(ImVec2(slot.x + 38.0f + nameWidth, slot.y));
                    if (GuiCards::button("##unequip", "QUITAR", ImVec2(70.0f, 24.0f), true, IM_COL32(150, 70, 70, 255))) message = ItemUse::unequip(data, storage, inventory, selection);
                    if (ImGui::IsMouseHoveringRect(slot, ImVec2(slot.x + 30.0f + nameWidth, slot.y + 26.0f))) ImGui::SetTooltip("%s", held->description.c_str());
                } else {
                    GuiCards::text(dl, ImVec2(slot.x, slot.y + 5.0f), GuiStyle::MUTED, "Sin objeto equipado (se da desde la mochila)", 0.8f);
                }
            }

            if (!embedded) {
                ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - 132.0f, pos.y + headerHeight - 30.0f));
                if (GuiCards::button("##moves", "MOVIMIENTOS", ImVec2(116.0f, 24.0f), true, IM_COL32(200, 70, 50, 255))) { editingMoves = true; MoveEditor::reset(); }
                ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - 132.0f - 176.0f, pos.y + headerHeight - 30.0f));
                if (GuiCards::button("##train", "EVs Y NATURALEZA", ImVec2(164.0f, 24.0f), true, IM_COL32(60, 130, 200, 255))) editingStats = true;
            }
            if (const Nature* nature = data.nature(owned.nature)) {
                std::snprintf(text, sizeof(text), "Naturaleza: %s", nature->name.c_str());
                GuiCards::text(dl, ImVec2(pos.x + 16.0f, pos.y + headerHeight - 26.0f), IM_COL32(255, 255, 255, 220), text, 0.85f);
            }

            const float left = pos.x + 16.0f, width = size.x - 32.0f;
            float y = pos.y + headerHeight + 10.0f;
            const bool compact = size.y < 640.0f;
            if (!compact && !embedded) {
                ImGui::SetCursorScreenPos(ImVec2(left, y));
                ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
                ImGui::PushTextWrapPos(left + width - ImGui::GetWindowPos().x);
                ImGui::TextUnformatted(species->description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                y = ImGui::GetCursorScreenPos().y + 6.0f;
            }

            // Abajo: habilidades y habilidad del mundo (alturas fijas); el resto es para el gráfico.
            const float bottomHeight = embedded ? 0.0f : compact ? 96.0f : 150.0f;
            const float statsAvail = pos.y + size.y - bottomHeight - y - 8.0f;
            if (owned.analyzed) drawStats(data, storage, inventory, owned, *species, ImVec2(left, y), statsAvail);
            else {
                ImGui::SetCursorScreenPos(ImVec2(left, y + 8.0f));
                ImGui::PushTextWrapPos(left + width - ImGui::GetWindowPos().x);
                GuiCards::muted("Sus estadísticas y su potencial están ocultos. Analízalo con la máquina de investigación.");
                ImGui::PopTextWrapPos();
            }

            if (embedded) { ImGui::Dummy(ImVec2(0.0f, 0.0f)); return; }
            ImGui::SetCursorScreenPos(ImVec2(left, pos.y + size.y - bottomHeight));
            worldSkillLine(dl, data, *species, !compact);
            abilityLine(dl, data.ability(owned.abilityId >= 0 ? owned.abilityId : species->abilityId), "Activa", IM_COL32(190, 80, 60, 255), !compact);
            if (species->passiveId >= 0) abilityLine(dl, data.ability(species->passiveId), "Pasiva", IM_COL32(130, 80, 190, 255), !compact);
            else abilityLine(dl, nullptr, "Pasiva", IM_COL32(90, 90, 100, 255), !compact);
            if (!message.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::SUCCESS);
                ImGui::TextUnformatted(message.c_str());
                ImGui::PopStyleColor();
            }
            // Los textos de arriba mueven el cursor con SetCursorScreenPos; sin un elemento al final ImGui aborta al cerrar la ventana.
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }

        // Letra del potencial en un círculo del color de su rango (sin escribir "potencial").
        inline void potentialBadge(ImDrawList* dl, const ImVec2& c, float r, const OwnedPokemon& owned) {
            const bool known = owned.analyzed;
            const IvRules::Rank rank = known ? IvRules::rank(owned.ivCaps) : IvRules::Rank::D;
            dl->AddCircleFilled(c, r, known ? GuiCards::rankColor(rank) : IM_COL32(90, 90, 100, 255), 32);
            dl->AddCircle(c, r, IM_COL32(255, 255, 255, 200), 32, 2.0f);
            GuiCards::centeredText(dl, c, IM_COL32(255, 255, 255, 255), known ? IvRules::label(rank) : "?", 1.0f);
        }

        // Botón "+" del objeto equipable (en el límite entre los datos y las estadísticas): sin objeto muestra un "+", con objeto solo su icono.
        // Al pulsarlo se elige entre los objetos equipables de la mochila; si no hay ninguno solo avisa.
        inline void drawHeldSlot(ImDrawList* dl, const GameData& data, PokemonStorage& storage, Inventory& inventory, const OwnedPokemon& owned, const ImVec2& c) {
            constexpr float R = 24.0f;
            const HeldItem* held = storage.heldOf(owned);
            ImGui::SetCursorScreenPos(ImVec2(c.x - R, c.y - R));
            const bool clicked = ImGui::InvisibleButton("##heldSlot", ImVec2(R * 2.0f, R * 2.0f));
            const bool hovered = ImGui::IsItemHovered();
            dl->AddCircleFilled(c, R, hovered ? IM_COL32(60, 64, 90, 255) : IM_COL32(34, 36, 52, 255), 32);
            dl->AddCircle(c, R, IM_COL32(190, 195, 220, 255), 32, 2.0f);
            if (held) {
                ItemIcon::held(dl, c, R - 5.0f, *held);
                if (hovered) ImGui::SetTooltip("%s\n%s", held->name.c_str(), held->description.c_str());
            } else GuiCards::centeredText(dl, c, IM_COL32(255, 255, 255, 255), "+", 1.6f);
            bool any = false;
            for (const Item& item : data.items) any = any || (ItemUse::isHeld(item) && inventory.count(item.id) > 0);
            if (clicked) {
                if (any || held) ImGui::OpenPopup("##heldChooser");
                else heldNotice = static_cast<float>(ImGui::GetTime()) + 2.5f;
            }
            if (static_cast<float>(ImGui::GetTime()) < heldNotice) GuiCards::centeredText(dl, ImVec2(c.x, c.y + R + 14.0f), IM_COL32(255, 120, 110, 255), "No tienes objetos equipables", 0.8f);
            ImGui::SetNextWindowPos(ImVec2(c.x + R + 8.0f, c.y - R));
            if (ImGui::BeginPopup("##heldChooser")) {
                for (const Item& item : data.items) {
                    if (!ItemUse::isHeld(item) || inventory.count(item.id) <= 0) continue;
                    const HeldItem* type = data.held(item.id);
                    char label[96];
                    std::snprintf(label, sizeof(label), "      %s  x%d", data.itemTitle(item, 1).c_str(), inventory.count(item.id));
                    const ImVec2 at = ImGui::GetCursorScreenPos();
                    if (ImGui::Selectable(label, held && type && held->id == type->id)) { message = ItemUse::equip(data, storage, inventory, item, selection); ImGui::CloseCurrentPopup(); }
                    if (type) ItemIcon::held(ImGui::GetWindowDrawList(), ImVec2(at.x + 12.0f, at.y + 9.0f), 8.0f, *type);
                }
                if (held && ImGui::Selectable("Quitar el objeto")) { message = ItemUse::unequip(data, storage, inventory, selection); ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
        }

        // Ficha del menú rápido. Pestañas fuera del recuadro, cada una con el suyo (la elegida se ve resaltada).
        // FICHA: sprite, nombre, tipos, nivel y PS; potencial (esquina izquierda) y pokéball de captura (esquina derecha); descripción y habilidades;
        // estadísticas base a la derecha; EVs y naturalezas debajo. MOVIMIENTOS: reordenar y sustituir arrastrando.
        inline void drawCard(const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress, const ImVec2& pos, const ImVec2& size) {
            (void)progress;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            static constexpr const char* NAMES[2] = { "FICHA", "MOVIMIENTOS" };
            const float tabH = 44.0f, tabW = 190.0f, tabGap = 10.0f;
            for (int i = 0; i < 2; ++i) {
                const ImVec2 a(pos.x + static_cast<float>(i) * (tabW + tabGap), pos.y), b(a.x + tabW, a.y + tabH);
                ImGui::SetCursorScreenPos(a);
                ImGui::PushID(500 + i);
                if (ImGui::InvisibleButton("##tab", ImVec2(tabW, tabH))) { tab = i; MoveEditor::reset(); }
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                const bool selected = tab == i;
                dl->AddRectFilled(a, b, selected ? IM_COL32(200, 70, 50, 255) : hovered ? IM_COL32(52, 55, 78, 255) : IM_COL32(30, 32, 46, 235), 10.0f);
                dl->AddRect(a, b, selected ? IM_COL32(255, 215, 90, 255) : IM_COL32(110, 115, 140, 255), 10.0f, 0, selected ? 3.0f : 1.5f);
                GuiCards::centeredText(dl, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), IM_COL32(255, 255, 255, selected ? 255 : 190), NAMES[i], 1.0f);
            }
            const ImVec2 card(pos.x, pos.y + tabH + 10.0f);
            const ImVec2 cardSize(size.x, size.y - tabH - 10.0f);
            GuiCards::panel(dl, card, ImVec2(card.x + cardSize.x, card.y + cardSize.y));
            const OwnedPokemon* found = ownedAt(storage, selection);
            const PokemonSpecies* species = found ? data.speciesById(found->speciesId) : nullptr;
            if (!species) {
                GuiCards::centeredText(dl, ImVec2(card.x + cardSize.x * 0.5f, card.y + cardSize.y * 0.5f), GuiStyle::MUTED, "Selecciona un pokémon", 0.95f);
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
                return;
            }
            const OwnedPokemon owned = *found;
            if (tab == 1) {
                MoveEditor::content(dl, data, storage, selection.inTeam, selection.index, owned, species, card, cardSize.x, cardSize.y, true);
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
                return;
            }

            // Todo se mide sobre el tamaño de la tarjeta: nada se sale de ella.
            const float topH = (std::clamp)(cardSize.y * 0.44f, 196.0f, 250.0f), leftW = cardSize.x * 0.5f;
            const ImU32 LineColor = IM_COL32(255, 255, 255, 45);
            char text[96];
            // Sprite (pequeño) y las dos filas de datos: nombre + tipos + nivel (el nivel a la derecha, alineado con el final de la barra de vida) y PS.
            const float spriteR = (std::min)(34.0f, topH * 0.17f);
            ItemIcon::creature(dl, ImVec2(card.x + 76.0f, card.y + 28.0f + spriteR + 14.0f), spriteR, species->id);
            const float rowX = card.x + 142.0f, rowR = card.x + leftW - 24.0f;
            const std::string name = title(*species, owned);
            const float nameScale = (std::min)(1.3f, (rowR - rowX - 150.0f) / (std::max)(1.0f, GuiCards::textWidth(name.c_str(), 1.0f)));
            GuiCards::text(dl, ImVec2(rowX, card.y + 22.0f), IM_COL32(255, 255, 255, 255), name.c_str(), (std::max)(0.8f, nameScale));
            float ix = rowX + GuiCards::textWidth(name.c_str(), (std::max)(0.8f, nameScale)) + 18.0f;
            for (const int typeId : species->types) if (const PokemonType* type = data.type(typeId)) { TypeIcons::draw(dl, ImVec2(ix + 12.0f, card.y + 34.0f), 12.0f, *type); ix += 30.0f; }
            std::snprintf(text, sizeof(text), "Nv. %d", owned.level);
            GuiCards::text(dl, ImVec2(rowR - GuiCards::textWidth(text, 1.1f), card.y + 24.0f), IM_COL32(255, 255, 255, 235), text, 1.1f);
            {
                const int maxHp = storage.maxHp(owned), hp = storage.hpOf(owned);
                const ImVec2 bar(rowX, card.y + 58.0f);
                const float barW = rowR - rowX;
                dl->AddRectFilled(bar, ImVec2(bar.x + barW, bar.y + 22.0f), IM_COL32(0, 0, 0, 130), 6.0f);
                const float f = storage.hpFraction(owned);
                if (f > 0.0f) dl->AddRectFilled(bar, ImVec2(bar.x + barW * f, bar.y + 22.0f), f > 0.5f ? IM_COL32(90, 210, 110, 255) : f > 0.2f ? IM_COL32(235, 200, 60, 255) : IM_COL32(230, 70, 70, 255), 6.0f);
                std::snprintf(text, sizeof(text), hp > 0 ? "PS %d / %d" : "PS 0 / %d  (debilitado)", hp > 0 ? hp : maxHp, maxHp);
                GuiCards::text(dl, ImVec2(bar.x + 10.0f, bar.y + 3.0f), IM_COL32(255, 255, 255, 245), text, 0.85f);
            }
            // Esquinas: potencial (izquierda) y pokéball con la que se capturó (derecha).
            potentialBadge(dl, ImVec2(card.x + 26.0f, card.y + 26.0f), 17.0f, owned);
            if (const PokeballType* ball = data.ball(owned.ballId)) ItemIcon::ball(dl, ImVec2(card.x + cardSize.x - 26.0f, card.y + 26.0f), 14.0f, PokeballStyle::look(ball->id));

            // Descripción (bajo el sprite y los PS) y habilidades (centradas, al pie del bloque).
            const float textX = card.x + 20.0f, textW = leftW - 40.0f;
            const float descY = card.y + 28.0f + spriteR * 2.0f + 22.0f;
            ImGui::SetCursorScreenPos(ImVec2(textX, descY));
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
            ImGui::PushTextWrapPos(textX + textW - ImGui::GetWindowPos().x);
            ImGui::TextUnformatted(species->description.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            {
                const Ability* active = data.ability(owned.abilityId >= 0 ? owned.abilityId : species->abilityId);
                const Ability* passive = species->passiveId >= 0 ? data.ability(species->passiveId) : nullptr;
                const float ay = card.y + topH - 30.0f, cx = card.x + leftW * 0.5f;
                std::string a = active ? (passive ? "Habilidad activa: " : "Habilidad: ") + active->name : std::string();
                std::string b = passive ? (active ? "Habilidad pasiva: " : "Habilidad: ") + passive->name : std::string();
                const float wa = a.empty() ? 0.0f : GuiCards::textWidth(a.c_str(), 0.95f), wb = b.empty() ? 0.0f : GuiCards::textWidth(b.c_str(), 0.95f), gap = !a.empty() && !b.empty() ? 30.0f : 0.0f;
                const float fit = (std::min)(1.0f, (leftW - 24.0f) / (std::max)(1.0f, wa + wb + gap));
                float x = cx - (wa + wb + gap) * fit * 0.5f;
                if (!a.empty()) { GuiCards::text(dl, ImVec2(x, ay), IM_COL32(255, 255, 255, 245), a.c_str(), 0.95f * fit); x += (wa + gap) * fit; }
                if (!b.empty()) GuiCards::text(dl, ImVec2(x, ay), IM_COL32(255, 255, 255, 245), b.c_str(), 0.95f * fit);
            }

            // Estadísticas base (gráfico) a la derecha.
            const float statsX = card.x + leftW + 40.0f, statsW = cardSize.x - leftW - 80.0f;
            if (owned.analyzed) drawStats(data, storage, inventory, owned, *species, ImVec2(statsX, card.y + 8.0f), topH - 16.0f, statsW);
            else GuiCards::centeredText(dl, ImVec2(statsX + statsW * 0.5f, card.y + topH * 0.5f), GuiStyle::MUTED, "Estadísticas ocultas: analízalo en la máquina de investigación", 0.9f);

            // Separaciones y objeto equipable en el límite entre datos y estadísticas.
            dl->AddLine(ImVec2(card.x + 16.0f, card.y + topH), ImVec2(card.x + cardSize.x - 16.0f, card.y + topH), LineColor, 2.0f);
            dl->AddLine(ImVec2(card.x + leftW, card.y + 16.0f), ImVec2(card.x + leftW, card.y + topH - 16.0f), LineColor, 2.0f);
            drawHeldSlot(dl, data, storage, inventory, owned, ImVec2(card.x + leftW, card.y + topH * 0.5f));

            // EVs (izquierda) y naturalezas (derecha), ajustados al espacio que queda.
            StatEditor::content(dl, data, storage, inventory, selection.inTeam, selection.index, owned, species, ImVec2(card.x, card.y + topH + 6.0f), cardSize.x, cardSize.y - topH - 8.0f, true);
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }
    }

    // ESC o el botón de volver cierran la pantalla (vuelven a 'back'); con un pokémon en la mano, primero lo suelta.
    inline void render(GameState& state, GameState back, const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress, InputDevice device, bool manage = false) {
        detail::manage = manage;
        if (!manage) detail::selecting = false; // (el arrastre del equipo sigue entre fotogramas)
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));

        // La selección debe seguir apuntando a algo que existe.
        if (!detail::ownedAt(storage, detail::selection)) detail::select({ true, 0 });

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float top = detail::MARGIN + 54.0f;
        const float barY = screen.y - detail::MARGIN - detail::BAR_HEIGHT + 4.0f;
        const float height = barY - top - 10.0f;

        if (GuiCards::backButton(ImVec2(detail::MARGIN, detail::MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(detail::MARGIN + 140.0f, detail::MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), manage ? "PC" : "POKÉMON", 1.6f);

        detail::slots.clear();
        const float teamWidth = 2.0f * (std::min)(46.0f, (height - 40.0f - 5.0f * detail::GAP) / 12.0f);
        detail::drawTeam(data, storage, ImVec2(detail::MARGIN, top), height);

        const float boxX = detail::MARGIN + teamWidth + 36.0f;
        if (manage) {
            const float boxWidth = screen.x - boxX - detail::DETAIL_WIDTH - detail::MARGIN - 28.0f;
            detail::drawBox(data, storage, ImVec2(boxX, top), ImVec2(boxWidth, height));
            detail::drawBar(data, storage, ImVec2(boxX, barY), boxWidth);
            detail::drawDetail(data, storage, inventory, progress, ImVec2(screen.x - detail::MARGIN - detail::DETAIL_WIDTH, top), ImVec2(detail::DETAIL_WIDTH, height));
        } else { // menú rápido: solo el equipo y, a su lado, la ficha con pestañas (los pokémon del PC se ven y se mueven desde la máquina del inicio)
            const float cardWidth = (std::min)(screen.x - boxX - detail::MARGIN, 1250.0f);
            detail::drawCard(data, storage, inventory, progress, ImVec2(boxX, top), ImVec2(cardWidth, height + detail::BAR_HEIGHT));
        }

        const ImVec2 hint(detail::MARGIN + 300.0f, detail::MARGIN - 4.0f);
        if (!manage) GuiCards::text(dl, ImVec2(hint.x, hint.y + 4.0f), GuiStyle::MUTED, "Arrastra un pokémon sobre otro para reordenar el equipo", 0.9f);
        else if (!isGamepad(device)) GuiCards::text(dl, ImVec2(hint.x, hint.y + 4.0f), GuiStyle::MUTED, "Arrastra un pokémon para colocarlo o intercambiarlo", 0.9f);
        else {
            const float w = GuiPrompts::draw(dl, hint, GuiPrompts::Action::INTERACT, "Coger", device);
            GuiPrompts::draw(dl, ImVec2(hint.x + w + 24.0f, hint.y), GuiPrompts::Action::JUMP, "Soltar en el destino", device);
        }
        detail::drawHeld(data, storage);
        detail::finishDrag(storage);

        if (detail::editingStats) {
            if (!StatEditor::render(data, storage, inventory, detail::selection.inTeam, detail::selection.index)) detail::editingStats = false;
            return; // el editor se queda con ESC y los clics este fotograma
        }
        if (detail::editingMoves) {
            if (!MoveEditor::render(data, storage, detail::selection.inTeam, detail::selection.index)) detail::editingMoves = false;
            return; // el editor se queda con ESC y los clics este fotograma
        }
        if (GuiInput::backPressed()) {
            if (detail::held.active) detail::held.active = false;
            else state = back;
        }
    }
}
