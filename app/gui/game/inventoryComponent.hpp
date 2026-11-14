#pragma once
#include "../../engine/world/rules/ivRules.hpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../style/icons/teamCircle.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/state/inventory.hpp"
#include "../../engine/world/state/itemUse.hpp"
#include "../../engine/world/state/playerProgress.hpp"
#include "../../engine/world/state/pokemonStorage.hpp"
#include "../../models/gameData.hpp"
#include "../../models/items/itemInfo.hpp"

// Mochila (objetos): pestañas de categoría a la izquierda, casillas de los objetos en el centro y, a la derecha, la
// ficha del objeto seleccionado (imagen, descripción y datos de la base de datos). Las herramientas se fabrican aquí.
namespace InventoryComponent {
    namespace detail {
        inline constexpr float MARGIN = 48.0f;
        inline constexpr float TAB_SIZE = 52.0f;
        inline constexpr float SECTION_HEIGHT = 36.0f;
        inline constexpr float DETAIL_WIDTH = 400.0f;
        inline constexpr float TILE_WIDTH = 96.0f;
        inline constexpr float TILE_HEIGHT = 108.0f;
        inline constexpr float GAP = 14.0f;

        inline int tab = 0;
        inline int selectedItem = -1; // id del objeto seleccionado

        struct Entry {
            const Item* item;
            int count;
        };

        // Objetos de una categoría: los que se tienen y, en herramientas, todas (para poder fabricarlas).
        inline std::vector<Entry> entries(const Inventory& inventory, int categoryId) {
            std::vector<Entry> list;
            inventory.forEach([&](const Item& item, int count) {
                if (item.categoryId == categoryId && (count > 0 || item.category == ItemCategory::TOOL)) list.push_back({ &item, count });
            });
            return list;
        }

        inline void heading(const char* value) {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::FOREGROUND);
            ImGui::TextUnformatted(value);
            ImGui::PopStyleColor();
        }

        // Orden de la mochila: las categorías agrupadas por apartado (en el orden en que aparece cada apartado) y por id dentro de cada uno.
        inline std::vector<int> order(const GameData& data) {
            std::vector<std::string> sections;
            for (const ItemCategoryInfo& category : data.categories) if (std::find(sections.begin(), sections.end(), category.section) == sections.end()) sections.push_back(category.section);
            std::vector<int> list;
            for (const std::string& section : sections) {
                for (size_t i = 0; i < data.categories.size(); ++i) if (data.categories[i].section == section) list.push_back(static_cast<int>(i));
            }
            return list;
        }

        // Fila superior: los apartados (una pastilla cada uno); debajo, las categorías del apartado elegido (la elegida con su nombre, el resto solo con el icono).
        inline void drawTabs(const GameData& data, const ImVec2& origin, float width) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const std::vector<int> sequence = order(data);
            const std::string& current = data.categories[tab].section;

            float x = origin.x;
            std::vector<std::string> done;
            for (const int index : sequence) {
                const std::string& section = data.categories[index].section;
                if (std::find(done.begin(), done.end(), section) != done.end()) continue;
                done.push_back(section);
                const float w = GuiCards::textWidth(section.c_str(), 1.0f) + 36.0f;
                ImGui::SetCursorScreenPos(ImVec2(x, origin.y));
                ImGui::PushID(static_cast<int>(done.size()) + 500);
                const bool clicked = ImGui::InvisibleButton("##section", ImVec2(w, SECTION_HEIGHT));
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                const bool selected = section == current;
                dl->AddRectFilled(ImVec2(x, origin.y), ImVec2(x + w, origin.y + SECTION_HEIGHT), selected ? GuiStyle::ACCENT : (hovered ? IM_COL32(60, 60, 78, 255) : GuiStyle::SURFACE), SECTION_HEIGHT * 0.5f);
                GuiCards::centeredText(dl, ImVec2(x + w * 0.5f, origin.y + SECTION_HEIGHT * 0.5f), IM_COL32(255, 255, 255, selected ? 255 : 190), section.c_str(), 1.0f);
                if (clicked && !selected) { tab = index; selectedItem = -1; }
                x += w + 10.0f;
            }

            x = origin.x;
            const float y = origin.y + SECTION_HEIGHT + 12.0f;
            for (const int index : sequence) {
                if (data.categories[index].section != current) continue;
                const ItemCategoryInfo& category = data.categories[index];
                const bool selected = index == tab;
                const float w = selected ? TAB_SIZE + GuiCards::textWidth(category.name.c_str(), 1.0f) + 22.0f : TAB_SIZE;
                if (x + w > origin.x + width) break;
                ImGui::SetCursorScreenPos(ImVec2(x, y));
                ImGui::PushID(index + 1000);
                if (ImGui::InvisibleButton("##tab", ImVec2(w, TAB_SIZE))) { tab = index; selectedItem = -1; }
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + TAB_SIZE), selected ? GuiStyle::ACCENT : (hovered ? IM_COL32(60, 60, 78, 255) : GuiStyle::SURFACE), TAB_SIZE * 0.5f);
                if (selected) dl->AddRect(ImVec2(x - 2.0f, y - 2.0f), ImVec2(x + w + 2.0f, y + TAB_SIZE + 2.0f), GuiCards::SELECT, TAB_SIZE * 0.5f + 2.0f, 0, 2.0f);
                if (const Item* icon = data.item(category.iconItemId)) ItemIcon::item(dl, ImVec2(x + TAB_SIZE * 0.5f, y + TAB_SIZE * 0.5f), TAB_SIZE * 0.3f, *icon, data);
                if (selected) GuiCards::text(dl, ImVec2(x + TAB_SIZE + 2.0f, y + TAB_SIZE * 0.5f - ImGui::GetFontSize() * 0.5f), IM_COL32(255, 255, 255, 255), category.name.c_str(), 1.0f);
                else if (hovered) ImGui::SetTooltip("%s", category.name.c_str());
                x += w + 10.0f;
            }
        }

        inline void drawGrid(const GameData& data, const std::vector<Entry>& list, const ImVec2& size) {
            ImGui::BeginChild("##grid", size, false);
            if (list.empty()) GuiCards::muted("Aquí no hay nada todavía.");

            const float available = ImGui::GetContentRegionAvail().x;
            const int columns = (std::max)(1, static_cast<int>((available + GAP) / (TILE_WIDTH + GAP)));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(GAP, GAP));
            char badge[16];
            for (int i = 0; i < static_cast<int>(list.size()); ++i) {
                const Item& item = *list[i].item;
                if (i % columns != 0) ImGui::SameLine();
                std::snprintf(badge, sizeof(badge), item.category == ItemCategory::TOOL ? "Nv %d" : "%d", list[i].count);
                ImGui::PushID(item.id);
                const bool picked = GuiCards::tile("##item", ImVec2(TILE_WIDTH, TILE_HEIGHT), item.id == selectedItem,
                    ItemIcon::categoryTone(item.category),
                    [&](ImDrawList* dl, const ImVec2& c, float r) { ItemIcon::item(dl, c, r, item, data, 1.0f, list[i].count); },
                    data.itemTitle(item, list[i].count).c_str(), badge, list[i].count <= 0);
                ImGui::PopID();
                if (picked) selectedItem = item.id;
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        // Mejora de una herramienta: el nivel siguiente, cada material con lo que se tiene y lo que hace falta, y el botón.
        inline void drawUpgrade(const GameData& data, Inventory& inventory, const Tool& tool) {
            heading("Mejora");
            const ToolTier* next = inventory.nextTier(tool);
            if (!next) {
                GuiCards::muted("Nivel máximo alcanzado.");
                return;
            }
            ImGui::TextUnformatted(tool.title(next->level).c_str());
            GuiCards::muted(next->description.c_str());
            char text[96];
            for (const RecipeIngredient& ingredient : next->cost) {
                const Item* material = data.materialItem(ingredient.materialId);
                const Material* info = data.material(ingredient.materialId);
                const int owned = material ? inventory.count(material->id) : 0;
                std::snprintf(text, sizeof(text), "%s  %d / %d", info ? info->name.c_str() : "?", owned, ingredient.quantity);
                ImGui::PushStyleColor(ImGuiCol_Text, owned >= ingredient.quantity ? GuiStyle::SUCCESS : GuiStyle::DANGER);
                ImGui::TextUnformatted(text);
                ImGui::PopStyleColor();
            }
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            if (GuiCards::button("##upgrade", "MEJORAR", ImVec2(180.0f, 38.0f), inventory.canUpgrade(tool))) inventory.upgrade(tool);
        }

        // Precios (pokémonedas) de compra y venta; vender se hace en la tienda. Las herramientas no se venden.
        inline std::string useMessage; // resultado del último uso (se borra al cambiar de objeto)
        inline int useMessageItem = -1;
        inline int vitaminTarget = -1; // pokémon del equipo elegido para la vitamina (luego se elige la estadística)

        // Objetos que se usan sobre un pokémon (pociones, revivir, caramelo, piedras evolutivas): el equipo, y al pulsar uno se usa sobre él.
        inline void drawUse(const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress, const Item& item, int count) {
            // Vitamina (sube IV), chapa plateada (un tope de IV a 31) y chapa dorada (todos los topes): las dos primeras piden después la estadística; la dorada actúa al elegir el pokémon.
            const TrainingItem* vitamin = nullptr;
            if (item.category == ItemCategory::TRAINING) {
                const TrainingItem* training = data.training(item.refId);
                if (training && (training->effect == TrainingItem::Effect::IV || training->effect == TrainingItem::Effect::IV_CAP)) vitamin = training;
            }
            const TrainingItem* gold = item.category == ItemCategory::TRAINING && data.training(item.refId) && data.training(item.refId)->effect == TrainingItem::Effect::IV_MAX ? data.training(item.refId) : nullptr;
            const bool silver = vitamin && vitamin->effect == TrainingItem::Effect::IV_CAP;
            if (ItemUse::kind(data, item) == ItemUse::Kind::NONE && !vitamin && !gold) return;
            if (useMessageItem != item.id) { useMessage.clear(); useMessageItem = item.id; vitaminTarget = -1; }
            heading("Usar sobre");
            ImDrawList* dl = ImGui::GetWindowDrawList();
            constexpr float R = 24.0f;
            const std::vector<OwnedPokemon>& team = storage.team();
            for (int i = 0; i < static_cast<int>(team.size()); ++i) {
                const PokemonSpecies* species = data.speciesById(team[i].speciesId);
                if (!species) continue;
                if (i > 0) ImGui::SameLine();
                const ImVec2 at = ImGui::GetCursorScreenPos();
                ImGui::PushID(2000 + i);
                const bool clicked = ImGui::InvisibleButton("##target", ImVec2(R * 2.0f + 6.0f, R * 2.0f + 14.0f));
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                bool works = count > 0 && ItemUse::applies(data, storage, progress, item, team[i]);
                if (vitamin || gold) {
                    works = false;
                    for (int stat = 0; stat < 6; ++stat) {
                        if (count <= 0) break;
                        if (silver || gold) works = works || team[i].ivCaps[stat] < IvRules::MAX_IV;
                        else works = works || team[i].ivs[stat] < team[i].ivCaps[stat];
                    }
                }
                const ImVec2 c(at.x + R + 3.0f, at.y + R + 10.0f);
                TeamCircle::draw(dl, c, R, data, *species, team[i].level, team[i].shiny, team[i].ballId, hovered && works, 0, works ? 1.0f : 0.35f, storage.fainted(team[i]));
                if (vitamin) {
                    if (clicked && works) { vitaminTarget = i; useMessage.clear(); }
                    if (vitaminTarget == i) dl->AddCircle(c, R + 3.0f, IM_COL32(255, 190, 80, 255), 32, 2.5f);
                } else if (gold) {
                    if (clicked && works && storage.raiseAllIvCaps(true, i)) {
                        inventory.add(item.id, -1);
                        useMessage = "¡Todos los IV de " + species->name + " pueden llegar a 31! Potencial S+";
                    }
                } else if (clicked && works) { useMessage = ItemUse::apply(data, storage, inventory, progress, item, { true, i }); useMessageItem = item.id; }
            }
            if (vitamin && vitaminTarget >= 0 && vitaminTarget < static_cast<int>(team.size())) {
                const OwnedPokemon& target = team[vitaminTarget];
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                for (int stat = 0; stat < 6; ++stat) {
                    const int missing = silver ? (target.ivCaps[stat] < IvRules::MAX_IV ? 1 : 0) : target.ivCaps[stat] - target.ivs[stat];
                    char label[64];
                    if (silver) std::snprintf(label, sizeof(label), "%s  tope %d%s", BaseStats::name(stat), target.ivCaps[stat], missing > 0 ? " > 31" : "");
                    else std::snprintf(label, sizeof(label), "%s  IV %d/%d", BaseStats::name(stat), target.ivs[stat], target.ivCaps[stat]);
                    ImGui::PushID(3000 + stat);
                    const bool pressed = GuiCards::button("##vit", label, ImVec2(170.0f, 28.0f), missing > 0 && inventory.count(item.id) > 0, silver ? IM_COL32(170, 180, 200, 255) : IM_COL32(230, 140, 40, 255));
                    if (pressed && (silver ? storage.raiseIvCap(true, vitaminTarget, stat) : storage.trainIv(true, vitaminTarget, stat, vitamin->amount))) {
                        inventory.add(item.id, -1);
                        useMessage = silver ? std::string(BaseStats::name(stat)) + ": ya puede llegar a 31" : std::string(BaseStats::name(stat)) + " entrenado";
                    }
                    ImGui::PopID();
                    if (stat % 2 == 0) ImGui::SameLine();
                }
            }
            if (!useMessage.empty()) GuiCards::muted(useMessage.c_str());
            else if (silver && count > 0) GuiCards::muted("Elige un pokémon y después la estadística cuyo tope subirá a 31.");
            else if (gold && count > 0) GuiCards::muted("Elige un pokémon: todos sus IV podrán llegar a 31.");
            else if (vitamin && count > 0) GuiCards::muted("Elige un pokémon y después la estadística a entrenar.");
            else if (count > 0) GuiCards::muted("Elige un pokémon del equipo (los apagados no sacan provecho).");
        }

        // Objetos equipables: el equipo; al pulsar un pokémon se lo equipa (el que llevaba otro objeto lo devuelve a la mochila). No cambia al acompañante.
        inline void drawEquip(const GameData& data, PokemonStorage& storage, Inventory& inventory, const Item& item, int count) {
            if (!ItemUse::isHeld(item)) return;
            if (useMessageItem != item.id) { useMessage.clear(); useMessageItem = item.id; vitaminTarget = -1; }
            heading("Equipar en");
            ImDrawList* dl = ImGui::GetWindowDrawList();
            constexpr float R = 24.0f;
            const std::vector<OwnedPokemon>& team = storage.team();
            for (int i = 0; i < static_cast<int>(team.size()); ++i) {
                const PokemonSpecies* species = data.speciesById(team[i].speciesId);
                if (!species) continue;
                if (i > 0) ImGui::SameLine();
                const ImVec2 at = ImGui::GetCursorScreenPos();
                ImGui::PushID(4000 + i);
                const bool clicked = ImGui::InvisibleButton("##holder", ImVec2(R * 2.0f + 6.0f, R * 2.0f + 14.0f));
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                const bool works = count > 0 && team[i].heldItemId != item.id;
                const ImVec2 c(at.x + R + 3.0f, at.y + R + 10.0f);
                TeamCircle::draw(dl, c, R, data, *species, team[i].level, team[i].shiny, team[i].ballId, hovered && works, 0, works ? 1.0f : 0.35f, storage.fainted(team[i]));
                if (const HeldItem* carried = storage.heldOf(team[i])) ItemIcon::held(dl, ImVec2(c.x + R * 0.75f, c.y + R * 0.75f), 8.0f, *carried);
                if (clicked && works) { useMessage = ItemUse::equip(data, storage, inventory, item, { true, i }); useMessageItem = item.id; }
            }
            if (!useMessage.empty()) GuiCards::muted(useMessage.c_str());
            else if (count > 0) GuiCards::muted("Elige un pokémon del equipo. Se quita desde su ficha.");
        }

        inline void drawTrade(const GameData& data, Inventory& inventory, const Item& item, int count) {
            if (item.category == ItemCategory::TOOL) return;
            heading("Precio");
            char text[96];
            std::snprintf(text, sizeof(text), "Compra: %d     Venta: %d  (pokémonedas)", item.buyPrice, item.sellPrice);
            ImGui::TextUnformatted(text);
            (void)inventory; (void)count; // la venta se hace en la tienda
        }

        inline void drawDetail(const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress, const Item* item, const std::string& category, const ImVec2& pos, const ImVec2& size) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y));
            if (!item) return;

            // Cabecera: imagen grande sobre el color de su categoría, nombre y cuántas se tienen.
            const float headerHeight = 190.0f;
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + headerHeight), GuiCards::withAlpha(ItemIcon::categoryTone(item->category), 200),
                              GuiCards::ROUNDING, ImDrawFlags_RoundCornersTop);
            const ImVec2 center(pos.x + size.x * 0.5f, pos.y + 78.0f);
            dl->AddCircleFilled(center, 62.0f, IM_COL32(0, 0, 0, 70), 48);
            ItemIcon::item(dl, center, 46.0f, *item, data, 1.0f, inventory.count(item->id));
            const int count = inventory.count(item->id);
            GuiCards::centeredText(dl, ImVec2(center.x, pos.y + 152.0f), IM_COL32(255, 255, 255, 255), data.itemTitle(*item, count).c_str(), 1.25f);
            char owned[64];
            if (const Tool* tool = data.toolOf(*item)) std::snprintf(owned, sizeof(owned), "%s  -  Nivel %d / %d", category.c_str(), count, tool->maxLevel());
            else std::snprintf(owned, sizeof(owned), "%s  -  Tienes %d", category.c_str(), count);
            GuiCards::centeredText(dl, ImVec2(center.x, pos.y + 175.0f), IM_COL32(255, 255, 255, 210), owned, 0.85f);

            ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f, pos.y + headerHeight + 12.0f));
            ImGui::BeginChild("##detail", ImVec2(size.x - 32.0f, size.y - headerHeight - 24.0f), false);

            // Descripción destacada.
            const std::string description = data.itemText(*item, count);
            const float wrap = ImGui::GetContentRegionAvail().x - 20.0f;
            const ImVec2 extent = ImGui::CalcTextSize(description.c_str(), nullptr, false, wrap);
            const ImVec2 box = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(box, ImVec2(box.x + wrap + 20.0f, box.y + extent.y + 20.0f), IM_COL32(255, 255, 255, 28), 8.0f);
            ImGui::SetCursorScreenPos(ImVec2(box.x + 10.0f, box.y + 10.0f));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
            ImGui::TextUnformatted(description.c_str());
            ImGui::PopTextWrapPos();
            ImGui::SetCursorScreenPos(ImVec2(box.x, box.y + extent.y + 24.0f));

            char text[96];
            if (item->category == ItemCategory::POKEBALL) {
                if (const PokeballType* type = data.ball(item->refId)) {
                    std::snprintf(text, sizeof(text), "Ratio de captura: %s", type->ratio().c_str());
                    ImGui::TextUnformatted(text);
                }
            } else if (const Tool* tool = data.toolOf(*item)) {
                const Skill* skill = data.skill(tool->skillId);
                std::snprintf(text, sizeof(text), "Habilidad: %s  -  Velocidad x%.1f", skill ? skill->name.c_str() : "?", tool->speed(count));
                ImGui::TextUnformatted(text);
                std::string works;
                for (const ResourceNodeType& node : data.nodes) {
                    if (node.skillId == tool->skillId && node.level <= count) works += (works.empty() ? "" : ", ") + node.name;
                }
                if (!works.empty()) GuiCards::muted(("Puede trabajar: " + works).c_str());
                drawUpgrade(data, inventory, *tool);
            }

            drawUse(data, storage, inventory, progress, *item, count);
            drawEquip(data, storage, inventory, *item, count);
            drawTrade(data, inventory, *item, count);

            if (const std::vector<std::string> sources = ItemInfo::sources(data, *item); !sources.empty()) {
                heading("Cómo conseguirlo");
                for (const std::string& line : sources) GuiCards::muted(line.c_str());
            }
            if (const std::vector<std::string> uses = ItemInfo::uses(data, *item); !uses.empty()) {
                heading("Para qué sirve");
                for (const std::string& line : uses) GuiCards::muted(line.c_str());
            }
            ImGui::EndChild();
        }
    }

    // ESC, I o el botón de volver del mando cierran la mochila (vuelven a 'back'). Q/E o L1/R1 cambian de pestaña.
    inline void render(GameState& state, GameState back, const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));

        const int categories = static_cast<int>(data.categories.size());
        if (categories == 0) {
            state = back;
            return;
        }
        detail::tab = (std::clamp)(detail::tab, 0, categories - 1);
        {
            const std::vector<int> sequence = detail::order(data); // Q / E recorren las categorías en el orden de la pantalla, pasando de un apartado al siguiente
            const int at = static_cast<int>(std::find(sequence.begin(), sequence.end(), detail::tab) - sequence.begin());
            if (ImGui::IsKeyPressed(ImGuiKey_Q, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)) { detail::tab = sequence[(at + categories - 1) % categories]; detail::selectedItem = -1; }
            if (ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) { detail::tab = sequence[(at + 1) % categories]; detail::selectedItem = -1; }
        }

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float top = detail::MARGIN + 54.0f;
        const float height = screen.y - top - detail::MARGIN - 28.0f;

        if (GuiCards::backButton(ImVec2(detail::MARGIN, detail::MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(detail::MARGIN + 140.0f, detail::MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "MOCHILA", 1.6f);
        char money[48];
        std::snprintf(money, sizeof(money), "Pokémonedas: %d", inventory.money());
        GuiCards::text(dl, ImVec2(screen.x - detail::MARGIN - GuiCards::textWidth(money, 1.2f), detail::MARGIN - 4.0f), IM_COL32(255, 215, 90, 255), money, 1.2f);

        const float gridWidth = screen.x - detail::MARGIN * 2.0f - detail::DETAIL_WIDTH - 28.0f;
        detail::drawTabs(data, ImVec2(detail::MARGIN, top), gridWidth);

        const ImVec2 gridPos(detail::MARGIN, top + detail::SECTION_HEIGHT + 12.0f + detail::TAB_SIZE + 16.0f);
        const ItemCategoryInfo& category = data.categories[detail::tab];
        const std::vector<detail::Entry> list = detail::entries(inventory, category.id);

        const detail::Entry* picked = nullptr;
        for (const detail::Entry& entry : list) if (entry.item->id == detail::selectedItem) picked = &entry;
        if (!picked && !list.empty()) {
            picked = &list.front();
            detail::selectedItem = picked->item->id;
        }

        ImGui::SetCursorScreenPos(gridPos);
        detail::drawGrid(data, list, ImVec2(gridWidth, top + height - gridPos.y));

        detail::drawDetail(data, storage, inventory, progress, picked ? picked->item : nullptr, category.name,
            ImVec2(screen.x - detail::MARGIN - detail::DETAIL_WIDTH, top), ImVec2(detail::DETAIL_WIDTH, height));

        GuiCards::text(dl, ImVec2(detail::MARGIN, screen.y - detail::MARGIN), GuiStyle::MUTED, "Q / E: categoría     ESC: volver", 0.9f);

        if (GuiInput::backPressed() || ImGui::IsKeyPressed(ImGuiKey_I, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false)) state = back;
    }
}
