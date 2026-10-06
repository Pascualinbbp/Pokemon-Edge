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
#include "../../engine/world/inventory.hpp"
#include "../../models/gameData.hpp"
#include "../../models/itemInfo.hpp"

// Mochila (objetos): pestañas de categoría a la izquierda, casillas de los objetos en el centro y, a la derecha, la
// ficha del objeto seleccionado (imagen, descripción y datos de la base de datos). Las herramientas se fabrican aquí.
namespace InventoryComponent {
    namespace detail {
        inline constexpr float MARGIN = 48.0f;
        inline constexpr float TAB_SIZE = 60.0f;
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

        inline void muted(const char* value) {
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
            ImGui::TextWrapped("%s", value);
            ImGui::PopStyleColor();
        }

        inline void heading(const char* value) {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::FOREGROUND);
            ImGui::TextUnformatted(value);
            ImGui::PopStyleColor();
        }

        inline void drawTabs(const GameData& data, const ImVec2& origin) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (size_t i = 0; i < data.categories.size(); ++i) {
                const ImVec2 a(origin.x, origin.y + static_cast<float>(i) * (TAB_SIZE + GAP));
                ImGui::SetCursorScreenPos(a);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::InvisibleButton("##tab", ImVec2(TAB_SIZE, TAB_SIZE))) {
                    tab = static_cast<int>(i);
                    selectedItem = -1;
                }
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();

                const bool selected = static_cast<int>(i) == tab;
                const ImVec2 c(a.x + TAB_SIZE * 0.5f, a.y + TAB_SIZE * 0.5f);
                dl->AddCircleFilled(c, TAB_SIZE * 0.5f, selected ? GuiStyle::ACCENT : (hovered ? IM_COL32(60, 60, 78, 255) : GuiStyle::SURFACE), 40);
                if (selected) dl->AddCircle(c, TAB_SIZE * 0.5f + 3.0f, GuiCards::SELECT, 40, 2.0f);
                ItemIcon::category(dl, c, TAB_SIZE * 0.3f, ItemCategoryText::parse(data.categories[i].name));
                if (hovered) ImGui::SetTooltip("%s", data.categories[i].label.c_str());
            }
        }

        inline void drawGrid(const GameData& data, const std::vector<Entry>& list, const ImVec2& size) {
            ImGui::BeginChild("##grid", size, false);
            if (list.empty()) muted("Aquí no hay nada todavía.");

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
                    [&](ImDrawList* dl, const ImVec2& c, float r) { ItemIcon::item(dl, c, r, item, data); },
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
                muted("Nivel máximo alcanzado.");
                return;
            }
            ImGui::TextUnformatted(next->name.c_str());
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

        // Precios (pokémonedas) y venta de las unidades que se tienen. Las herramientas no se venden.
        inline void drawTrade(const GameData& data, Inventory& inventory, const Item& item, int count) {
            if (item.category == ItemCategory::TOOL) return;
            heading("Precio");
            char text[96];
            std::snprintf(text, sizeof(text), "Compra: %d     Venta: %d  (pokémonedas)", item.buyPrice, item.sellPrice);
            ImGui::TextUnformatted(text);
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            const bool canSell = item.sellPrice > 0 && count > 0;
            if (GuiCards::button("##sell1", "VENDER 1", ImVec2(150.0f, 34.0f), canSell, IM_COL32(190, 120, 30, 255))) inventory.sell(item.id, 1);
            ImGui::SameLine();
            if (GuiCards::button("##sellAll", "VENDER TODO", ImVec2(170.0f, 34.0f), canSell, IM_COL32(190, 120, 30, 255))) inventory.sell(item.id, count);
        }

        inline void drawDetail(const GameData& data, Inventory& inventory, const Item* item, const std::string& category, const ImVec2& pos, const ImVec2& size) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y));
            if (!item) return;

            // Cabecera: imagen grande sobre el color de su categoría, nombre y cuántas se tienen.
            const float headerHeight = 190.0f;
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + headerHeight), GuiCards::withAlpha(ItemIcon::categoryTone(item->category), 200),
                              GuiCards::ROUNDING, ImDrawFlags_RoundCornersTop);
            const ImVec2 center(pos.x + size.x * 0.5f, pos.y + 78.0f);
            dl->AddCircleFilled(center, 62.0f, IM_COL32(0, 0, 0, 70), 48);
            ItemIcon::item(dl, center, 46.0f, *item, data);
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
                    std::snprintf(text, sizeof(text), "Ratio de captura: x%.1f", type->captureMultiplier);
                    ImGui::TextUnformatted(text);
                }
            } else if (const Tool* tool = data.toolOf(*item)) {
                const Skill* skill = data.skill(tool->skillId);
                const ToolTier* tier = tool->tier(count);
                std::snprintf(text, sizeof(text), "Habilidad: %s  -  Velocidad x%.1f", skill ? skill->name.c_str() : "?", tier ? tier->speed : 1.0f);
                ImGui::TextUnformatted(text);
                std::string works;
                for (const ResourceNodeType& node : data.nodes) {
                    if (node.skillId == tool->skillId && node.level <= count) works += (works.empty() ? "" : ", ") + node.name;
                }
                if (!works.empty()) muted(("Puede trabajar: " + works).c_str());
                drawUpgrade(data, inventory, *tool);
            }

            drawTrade(data, inventory, *item, count);

            if (const std::vector<std::string> sources = ItemInfo::sources(data, *item); !sources.empty()) {
                heading("Cómo conseguirlo");
                for (const std::string& line : sources) muted(line.c_str());
            }
            if (const std::vector<std::string> uses = ItemInfo::uses(data, *item); !uses.empty()) {
                heading("Para qué sirve");
                for (const std::string& line : uses) muted(line.c_str());
            }
            ImGui::EndChild();
        }
    }

    // ESC, I o el botón de volver del mando cierran la mochila (vuelven a 'back'). Q/E o L1/R1 cambian de pestaña.
    inline void render(GameState& state, GameState back, const GameData& data, Inventory& inventory) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));

        const int categories = static_cast<int>(data.categories.size());
        if (categories == 0) {
            state = back;
            return;
        }
        detail::tab = (std::clamp)(detail::tab, 0, categories - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)) { detail::tab = (detail::tab + categories - 1) % categories; detail::selectedItem = -1; }
        if (ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) { detail::tab = (detail::tab + 1) % categories; detail::selectedItem = -1; }

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float top = detail::MARGIN + 54.0f;
        const float height = screen.y - top - detail::MARGIN - 28.0f;

        if (GuiCards::backButton(ImVec2(detail::MARGIN, detail::MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(detail::MARGIN + 140.0f, detail::MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "MOCHILA", 1.6f);
        char money[48];
        std::snprintf(money, sizeof(money), "Pokémonedas: %d", inventory.money());
        GuiCards::text(dl, ImVec2(screen.x - detail::MARGIN - GuiCards::textWidth(money, 1.2f), detail::MARGIN - 4.0f), IM_COL32(255, 215, 90, 255), money, 1.2f);

        detail::drawTabs(data, ImVec2(detail::MARGIN, top));

        const ImVec2 gridPos(detail::MARGIN + detail::TAB_SIZE + 36.0f, top);
        const float gridWidth = screen.x - gridPos.x - detail::DETAIL_WIDTH - detail::MARGIN - 28.0f;
        const ItemCategoryInfo& category = data.categories[detail::tab];
        const std::vector<detail::Entry> list = detail::entries(inventory, category.id);

        const detail::Entry* picked = nullptr;
        for (const detail::Entry& entry : list) if (entry.item->id == detail::selectedItem) picked = &entry;
        if (!picked && !list.empty()) {
            picked = &list.front();
            detail::selectedItem = picked->item->id;
        }

        GuiCards::text(dl, gridPos, IM_COL32(255, 255, 255, 255), category.label.c_str(), 1.2f);
        ImGui::SetCursorScreenPos(ImVec2(gridPos.x, gridPos.y + 34.0f));
        detail::drawGrid(data, list, ImVec2(gridWidth, height - 34.0f));

        detail::drawDetail(data, inventory, picked ? picked->item : nullptr, category.label,
            ImVec2(screen.x - detail::MARGIN - detail::DETAIL_WIDTH, top), ImVec2(detail::DETAIL_WIDTH, height));

        GuiCards::text(dl, ImVec2(detail::MARGIN, screen.y - detail::MARGIN), GuiStyle::MUTED, "Q / E: categoría     ESC: volver", 0.9f);

        if (GuiInput::backPressed() || ImGui::IsKeyPressed(ImGuiKey_I, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false)) state = back;
    }
}
