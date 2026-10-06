#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiStyle.hpp"
#include "../style/itemIcon.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/inventory.hpp"
#include "../../engine/world/pokemonStorage.hpp"
#include "../../models/gameData.hpp"

// Inventario: una pestaña por categoría de objeto (las de la base de datos), más el equipo (6) y el PC.
// A la izquierda la lista de lo que hay en la pestaña; a la derecha, la imagen de lo seleccionado y su descripción.
namespace InventoryComponent {
    namespace detail {
        inline constexpr float WIDTH = 980.0f;
        inline constexpr float HEIGHT = 400.0f;
        inline constexpr float LIST_WIDTH = 400.0f;
        inline constexpr float ROW_HEIGHT = 44.0f;
        inline constexpr float ICON_RADIUS = 15.0f;
        inline constexpr float BIG_ICON_RADIUS = 70.0f;
        inline constexpr float TAB_WIDTH = 150.0f;
        inline constexpr float TAB_HEIGHT = 36.0f;

        inline int selectedTab = 0;
        inline int selectedRow = 0;

        inline bool tabButton(const char* label, bool selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(selected ? GuiStyle::ACCENT : GuiStyle::SURFACE));
            const bool clicked = ImGui::Button(label, ImVec2(TAB_WIDTH, TAB_HEIGHT));
            ImGui::PopStyleColor();
            return clicked;
        }

        inline void muted(const char* text) {
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
            ImGui::TextWrapped("%s", text);
            ImGui::PopStyleColor();
        }

        // Fila seleccionable de la lista con icono, nombre y detalle a la derecha. Devuelve true si se ha elegido.
        template <typename Icon>
        inline bool row(int index, const char* name, const char* detail, Icon drawIcon) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;

            ImGui::PushID(index);
            const bool clicked = ImGui::Selectable("##row", index == selectedRow, 0, ImVec2(0.0f, ROW_HEIGHT));
            ImGui::PopID();

            drawIcon(dl, ImVec2(p.x + ICON_RADIUS + 8.0f, p.y + ROW_HEIGHT * 0.5f), ICON_RADIUS);
            const float textY = p.y + (ROW_HEIGHT - ImGui::GetFontSize()) * 0.5f;
            dl->AddText(ImVec2(p.x + ICON_RADIUS * 2.0f + 22.0f, textY), GuiStyle::FOREGROUND, name);
            if (detail[0]) {
                const float w = ImGui::CalcTextSize(detail).x;
                dl->AddText(ImVec2(p.x + width - w - 12.0f, textY), GuiStyle::MUTED, detail);
            }
            return clicked;
        }

        // Panel derecho: imagen grande, nombre y descripción debajo.
        template <typename Icon>
        inline void detailHeader(const char* name, Icon drawIcon) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            const ImVec2 center(p.x + width * 0.5f, p.y + BIG_ICON_RADIUS + 12.0f);

            dl->AddCircleFilled(center, BIG_ICON_RADIUS + 14.0f, GuiStyle::SURFACE, 48);
            dl->AddCircle(center, BIG_ICON_RADIUS + 14.0f, GuiStyle::ACCENT, 48, 2.0f);
            drawIcon(dl, center, BIG_ICON_RADIUS);

            ImGui::Dummy(ImVec2(0.0f, (BIG_ICON_RADIUS + 14.0f) * 2.0f + 14.0f));
            GuiLayout::centeredText(name);
            GuiLayout::gap(GuiLayout::GAP_SMALL);
        }

        inline std::string skillsText(const GameData& data, int speciesId) {
            std::string text;
            if (const PokemonSpecies* species = data.speciesById(speciesId)) {
                for (const SpeciesSkill& entry : species->skills) {
                    if (const Skill* skill = data.skill(entry.skillId)) text += (text.empty() ? "" : ", ") + skill->name;
                }
            }
            return text.empty() ? "Sin habilidades de trabajo" : text;
        }

        inline const char* speciesName(const GameData& data, int speciesId) {
            const PokemonSpecies* species = data.speciesById(speciesId);
            return species ? species->name.c_str() : "?";
        }

        // --- Pestaña de una categoría de objetos ---
        inline void drawItems(const GameData& data, const Inventory& inventory, int categoryId) {
            std::vector<const Item*> items;
            std::vector<int> counts;
            inventory.forEach([&](const Item& item, int count) {
                if (item.categoryId == categoryId && count > 0) {
                    items.push_back(&item);
                    counts.push_back(count);
                }
            });
            selectedRow = items.empty() ? 0 : (std::min)(selectedRow, static_cast<int>(items.size()) - 1);

            ImGui::BeginChild("##list", ImVec2(LIST_WIDTH, 0.0f), true);
            if (items.empty()) muted("Vacío");
            char amount[16];
            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                std::snprintf(amount, sizeof(amount), "x%d", counts[i]);
                if (row(i, data.itemName(*items[i]).c_str(), amount,
                        [&](ImDrawList* dl, const ImVec2& c, float r) { ItemIcon::item(dl, c, r, *items[i], data); })) selectedRow = i;
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##detail", ImVec2(0.0f, 0.0f), true);
            if (!items.empty()) {
                const Item& item = *items[selectedRow];
                detailHeader(data.itemName(item).c_str(), [&](ImDrawList* dl, const ImVec2& c, float r) { ItemIcon::item(dl, c, r, item, data); });
                muted(data.itemDescription(item).c_str());
                if (item.category == ItemCategory::POKEBALL) {
                    if (const PokeballType* type = data.ball(item.refId)) {
                        char text[48];
                        std::snprintf(text, sizeof(text), "Ratio de captura x%.1f", type->captureMultiplier);
                        ImGui::TextUnformatted(text);
                    }
                }
            }
            ImGui::EndChild();
        }

        // --- Pestañas de pokémon: equipo y PC (misma lista con distintas acciones) ---
        inline void drawPokemon(const GameData& data, PokemonStorage& storage, bool inTeam) {
            const std::vector<int>& list = inTeam ? storage.team() : storage.pc();
            selectedRow = list.empty() ? 0 : (std::min)(selectedRow, static_cast<int>(list.size()) - 1);

            ImGui::BeginChild("##list", ImVec2(LIST_WIDTH, 0.0f), true);
            char header[48];
            if (inTeam) std::snprintf(header, sizeof(header), "Equipo: %d / %d", static_cast<int>(list.size()), PokemonStorage::TEAM_SIZE);
            else std::snprintf(header, sizeof(header), "PC: %d / %d", static_cast<int>(list.size()), PokemonStorage::PC_CAPACITY);
            muted(header);
            if (list.empty()) muted("Vacío");
            for (int i = 0; i < static_cast<int>(list.size()); ++i) {
                if (row(i, speciesName(data, list[i]), inTeam && i == 0 ? "Líder" : "",
                        [&](ImDrawList* dl, const ImVec2& c, float r) { ItemIcon::creature(dl, c, r, list[i]); })) selectedRow = i;
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##detail", ImVec2(0.0f, 0.0f), true);
            if (!list.empty()) {
                const int speciesId = list[selectedRow];
                detailHeader(speciesName(data, speciesId), [&](ImDrawList* dl, const ImVec2& c, float r) { ItemIcon::creature(dl, c, r, speciesId); });
                muted(("Habilidades: " + skillsText(data, speciesId)).c_str());
                GuiLayout::gap(GuiLayout::GAP_SMALL);

                if (inTeam) {
                    if (selectedRow > 0 && ImGui::Button("Hacer líder", ImVec2(160.0f, 36.0f))) storage.makeLead(selectedRow);
                    if (selectedRow > 0) ImGui::SameLine();
                    ImGui::BeginDisabled(storage.pcFull());
                    if (ImGui::Button("Enviar al PC", ImVec2(160.0f, 36.0f))) storage.sendToPc(selectedRow);
                    ImGui::EndDisabled();
                } else {
                    ImGui::BeginDisabled(storage.teamFull());
                    if (ImGui::Button("Añadir al equipo", ImVec2(180.0f, 36.0f))) storage.sendToTeam(selectedRow);
                    ImGui::EndDisabled();
                }
            }
            ImGui::EndChild();
        }
    }

    // ESC, I o el botón de volver del mando cierran el inventario.
    inline void render(GameState& state, const GameData& data, const Inventory& inventory, PokemonStorage& storage) {
        GuiLayout::dimBackground();

        GuiLayout::beginAt(0.08f);
        GuiLayout::centeredText("INVENTARIO");
        GuiLayout::gap(GuiLayout::GAP_MEDIUM);

        const int categories = static_cast<int>(data.categories.size());
        const int tabs = categories + 2; // categorías + equipo + PC
        detail::selectedTab = (std::min)(detail::selectedTab, tabs - 1);

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        GuiLayout::centerX((detail::TAB_WIDTH + spacing) * static_cast<float>(tabs) - spacing);
        for (int i = 0; i < tabs; ++i) {
            if (i > 0) ImGui::SameLine();
            const char* label = i < categories ? data.categories[i].label.c_str() : i == categories ? "Equipo" : "PC";
            if (detail::tabButton(label, i == detail::selectedTab) && i != detail::selectedTab) {
                detail::selectedTab = i;
                detail::selectedRow = 0;
            }
        }
        GuiLayout::gap(GuiLayout::GAP_SMALL);

        GuiLayout::centerX(detail::WIDTH);
        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, GuiStyle::PANEL);
        ImGui::BeginChild("##inventory", ImVec2(detail::WIDTH, detail::HEIGHT), false, ImGuiWindowFlags_NoScrollbar);
        if (detail::selectedTab < categories) detail::drawItems(data, inventory, data.categories[detail::selectedTab].id);
        else detail::drawPokemon(data, storage, detail::selectedTab == categories);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        GuiLayout::gap(GuiLayout::GAP_SMALL);
        GuiLayout::centeredText("ESC / I: cerrar", GuiStyle::MUTED);

        if (GuiInput::backPressed() || ImGui::IsKeyPressed(ImGuiKey_I, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false)) {
            state = GameState::PLAYING;
        }
    }
}
