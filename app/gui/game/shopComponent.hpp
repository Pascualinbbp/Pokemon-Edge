#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/core/gameEngine.hpp"

// Tienda del tendero. Arriba COMPRAR / VENDER; a la izquierda las categorías (Pokéballs, medicinas...) y a la derecha los objetos de la categoría.
// Pulsar COMPRAR / VENDER en un objeto pregunta la cantidad; VENDER TODO vende todas las unidades. Lo que hay a la venta lo decide la base de datos
// (precio de compra > 0 en el apartado Pokémon); lo que se compra, el precio de venta.
namespace ShopComponent {
    namespace detail {
        struct State {
            bool selling = false;
            int category = -1;      // -1 = todas
            int dialogItem = -1;    // objeto del que se pregunta la cantidad (-1 = sin diálogo)
            int quantity = 1;
            std::string message;
            ImU32 messageColor = IM_COL32(120, 230, 150, 255);
            float messageTime = 0.0f;
        };
        inline State& state() { static State s; return s; }

        struct Row { const Item* item; int owned; };

        inline void say(State& s, const std::string& text, bool good) {
            s.message = text;
            s.messageColor = good ? IM_COL32(120, 230, 150, 255) : IM_COL32(250, 120, 110, 255);
            s.messageTime = 3.0f;
        }
    }

    inline void reset() { detail::state() = {}; }

    inline void render(GameState& gameState, GameState back, GameEngine& engine) {
        using namespace detail;
        State& s = state();
        const GameData& data = engine.data();
        Inventory& inventory = engine.inventory();
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), screen, IM_COL32(0, 0, 0, 160));

        const float width = (std::min)(1020.0f, screen.x - 60.0f), height = (std::min)(680.0f, screen.y - 60.0f);
        const ImVec2 origin((screen.x - width) * 0.5f, (screen.y - height) * 0.5f);
        GuiCards::panel(dl, origin, ImVec2(origin.x + width, origin.y + height));
        GuiCards::text(dl, ImVec2(origin.x + 26.0f, origin.y + 18.0f), IM_COL32(255, 255, 255, 255), "Tienda Pokémon", 1.5f);
        char money[48];
        std::snprintf(money, sizeof(money), "%d pokémonedas", inventory.money());
        GuiCards::chip(dl, ImVec2(origin.x + width - 26.0f - GuiCards::textWidth(money, 0.95f) - 16.0f, origin.y + 22.0f), money, IM_COL32(190, 140, 20, 255), 0.95f);

        // Lo que hay en la pestaña actual y sus categorías.
        std::vector<Row> all;
        if (!s.selling) {
            for (const Item* item : engine.shopStock()) all.push_back({ item, inventory.count(item->id) });
        } else {
            inventory.forEach([&](const Item& item, int count) { if (item.sellPrice > 0 && count > 0) all.push_back({ &item, count }); });
        }
        std::vector<int> categories;
        for (const Row& row : all) if (std::find(categories.begin(), categories.end(), row.item->categoryId) == categories.end()) categories.push_back(row.item->categoryId);
        std::sort(categories.begin(), categories.end());
        if (s.category >= 0 && std::find(categories.begin(), categories.end(), s.category) == categories.end()) s.category = -1;
        std::vector<Row> rows;
        for (const Row& row : all) if (s.category < 0 || row.item->categoryId == s.category) rows.push_back(row);

        const bool dialog = s.dialogItem >= 0;
        if (dialog) ImGui::BeginDisabled();

        // Pestañas
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 26.0f, origin.y + 62.0f));
        if (GuiCards::button("##buy", "COMPRAR", ImVec2(150.0f, 34.0f), true, s.selling ? IM_COL32(70, 70, 92, 255) : GuiStyle::ACCENT)) { s.selling = false; s.category = -1; }
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 190.0f, origin.y + 62.0f));
        if (GuiCards::button("##sell", "VENDER", ImVec2(150.0f, 34.0f), true, s.selling ? GuiStyle::ACCENT : IM_COL32(70, 70, 92, 255))) { s.selling = true; s.category = -1; }

        // Categorías (columna izquierda)
        const float top = origin.y + 112.0f, bottom = origin.y + height - 64.0f, sideWidth = 200.0f;
        float cy = top;
        const auto categoryButton = [&](int id, const std::string& name, const Item* icon) {
            ImGui::SetCursorScreenPos(ImVec2(origin.x + 26.0f, cy));
            ImGui::PushID(id + 5000);
            const bool clicked = ImGui::InvisibleButton("##category", ImVec2(sideWidth, 40.0f));
            const bool over = ImGui::IsItemHovered();
            ImGui::PopID();
            dl->AddRectFilled(ImVec2(origin.x + 26.0f, cy), ImVec2(origin.x + 26.0f + sideWidth, cy + 40.0f), s.category == id ? GuiStyle::ACCENT : over ? IM_COL32(60, 60, 84, 255) : GuiStyle::SURFACE, 12.0f);
            float tx = origin.x + 40.0f;
            if (icon) { ItemIcon::item(dl, ImVec2(origin.x + 48.0f, cy + 20.0f), 13.0f, *icon, data); tx = origin.x + 70.0f; }
            GuiCards::text(dl, ImVec2(tx, cy + 10.0f), IM_COL32(255, 255, 255, 255), name.c_str(), 0.9f);
            cy += 46.0f;
            if (clicked) s.category = id;
        };
        categoryButton(-1, "Todo", nullptr);
        for (const int id : categories) {
            for (const ItemCategoryInfo& category : data.categories) {
                if (category.id != id) continue;
                categoryButton(id, category.name, category.iconItemId >= 0 ? data.item(category.iconItemId) : nullptr);
                break;
            }
        }

        // Lista de objetos (a la derecha de las categorías)
        const float listLeft = origin.x + 26.0f + sideWidth + 16.0f, listWidth = origin.x + width - 26.0f - listLeft, listHeight = bottom - top;
        constexpr float CARD_H = 70.0f, GAP = 8.0f;
        const float buttonsWidth = s.selling ? 250.0f : 140.0f;
        ImGui::SetCursorScreenPos(ImVec2(listLeft, top));
        ImGui::BeginChild("##shopList", ImVec2(listWidth, listHeight), false);
        ImDrawList* cl = ImGui::GetWindowDrawList();
        const ImVec2 base = ImGui::GetCursorScreenPos();
        const float cardW = listWidth - 14.0f;
        const Item* hoveredItem = nullptr;
        for (size_t i = 0; i < rows.size(); ++i) {
            const Item& item = *rows[i].item;
            const float y = base.y + static_cast<float>(i) * (CARD_H + GAP);
            ImGui::PushID(static_cast<int>(item.id));
            ImGui::SetCursorScreenPos(ImVec2(base.x, y));
            ImGui::InvisibleButton("##info", ImVec2(cardW - buttonsWidth - 8.0f, CARD_H)); // zona de información (sin acción): solo para señalar
            const bool over = ImGui::IsItemHovered();
            if (over) hoveredItem = &item;
            cl->AddRectFilled(ImVec2(base.x, y), ImVec2(base.x + cardW, y + CARD_H), over ? IM_COL32(60, 60, 84, 255) : GuiStyle::SURFACE, 14.0f);
            ItemIcon::item(cl, ImVec2(base.x + 38.0f, y + CARD_H * 0.5f), 22.0f, item, data);
            GuiCards::text(cl, ImVec2(base.x + 76.0f, y + 10.0f), IM_COL32(255, 255, 255, 255), data.itemName(item).c_str(), 1.0f);
            const int price = s.selling ? item.sellPrice : item.buyPrice;
            char line[64];
            std::snprintf(line, sizeof(line), "%d pokémonedas", price);
            const bool affordable = s.selling || inventory.money() >= price;
            GuiCards::text(cl, ImVec2(base.x + 76.0f, y + 38.0f), affordable ? IM_COL32(255, 215, 90, 255) : IM_COL32(250, 120, 110, 255), line, 0.85f);
            std::snprintf(line, sizeof(line), "Tienes %d", rows[i].owned);
            GuiCards::text(cl, ImVec2(base.x + 76.0f + 200.0f, y + 38.0f), IM_COL32(255, 255, 255, 190), line, 0.85f);
            const float by = y + (CARD_H - 38.0f) * 0.5f;
            if (!s.selling) {
                ImGui::SetCursorScreenPos(ImVec2(base.x + cardW - 140.0f - 6.0f, by));
                if (GuiCards::button("##buyItem", "COMPRAR", ImVec2(140.0f, 38.0f), true, IM_COL32(60, 150, 90, 255))) { s.dialogItem = item.id; s.quantity = 1; }
            } else {
                ImGui::SetCursorScreenPos(ImVec2(base.x + cardW - 250.0f - 6.0f, by));
                if (GuiCards::button("##sellItem", "VENDER", ImVec2(110.0f, 38.0f), true, IM_COL32(190, 120, 30, 255))) { s.dialogItem = item.id; s.quantity = 1; }
                ImGui::SetCursorScreenPos(ImVec2(base.x + cardW - 134.0f - 6.0f, by));
                if (GuiCards::button("##sellAll", "VENDER TODO", ImVec2(134.0f, 38.0f), true, IM_COL32(190, 120, 30, 255))) {
                    const int units = rows[i].owned, got = inventory.sell(item.id, units);
                    if (got > 0) say(s, std::to_string(units) + "x " + data.itemName(item) + " vendido por " + std::to_string(got), true);
                }
            }
            ImGui::PopID();
        }
        if (rows.empty()) {
            ImGui::SetCursorScreenPos(base);
            ImGui::Dummy(ImVec2(listWidth - 20.0f, 40.0f));
            GuiCards::text(cl, ImVec2(base.x + 8.0f, base.y + 8.0f), GuiStyle::MUTED, s.selling ? "No llevas nada que se pueda vender." : "No hay nada a la venta.", 1.0f);
        } else {
            ImGui::SetCursorScreenPos(ImVec2(base.x, base.y + static_cast<float>(rows.size()) * (CARD_H + GAP)));
            ImGui::Dummy(ImVec2(1.0f, 1.0f)); // el scroll termina siempre en un elemento
        }
        ImGui::EndChild();

        // Pie: resultado de la última operación, descripción del objeto señalado o ayuda.
        const float footY = origin.y + height - 48.0f;
        if (s.messageTime > 0.0f) {
            s.messageTime -= ImGui::GetIO().DeltaTime;
            if (s.messageTime <= 0.0f) s.message.clear();
        }
        if (!s.message.empty()) GuiCards::text(dl, ImVec2(origin.x + 26.0f, footY), s.messageColor, s.message.c_str(), 1.0f);
        else if (hoveredItem) GuiCards::text(dl, ImVec2(origin.x + 26.0f, footY), IM_COL32(255, 255, 255, 220), hoveredItem->description.c_str(), 0.9f);
        else GuiCards::text(dl, ImVec2(origin.x + 26.0f, footY), GuiStyle::MUTED, "ESC: salir", 0.9f);
        ImGui::SetCursorScreenPos(ImVec2(origin.x + width - 26.0f - 120.0f, origin.y + height - 54.0f));
        const bool exit = GuiCards::button("##exit", "SALIR", ImVec2(120.0f, 36.0f), true, IM_COL32(70, 70, 92, 255));
        if (dialog) ImGui::EndDisabled();

        // Diálogo de cantidad
        if (dialog) {
            const Item* item = data.item(s.dialogItem);
            if (!item) { s.dialogItem = -1; return; }
            const int unit = s.selling ? item->sellPrice : item->buyPrice;
            const int limit = s.selling ? inventory.count(item->id) : (std::max)(1, (std::min)(99, inventory.money() / (std::max)(1, unit)));
            s.quantity = (std::clamp)(s.quantity + static_cast<int>(ImGui::GetIO().MouseWheel), 1, (std::max)(1, limit));
            ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), screen, IM_COL32(0, 0, 0, 120));
            const float dw = 520.0f, dh = 290.0f;
            const ImVec2 d((screen.x - dw) * 0.5f, (screen.y - dh) * 0.5f);
            GuiCards::panel(dl, d, ImVec2(d.x + dw, d.y + dh), IM_COL32(30, 30, 44, 250));
            ItemIcon::item(dl, ImVec2(d.x + 52.0f, d.y + 56.0f), 28.0f, *item, data);
            GuiCards::text(dl, ImVec2(d.x + 96.0f, d.y + 22.0f), IM_COL32(255, 255, 255, 255), data.itemName(*item).c_str(), 1.2f);
            char line[96];
            std::snprintf(line, sizeof(line), "%s: %d pokémonedas c/u   -   Tienes %d", s.selling ? "Venta" : "Precio", unit, inventory.count(item->id));
            GuiCards::text(dl, ImVec2(d.x + 96.0f, d.y + 54.0f), IM_COL32(255, 215, 90, 255), line, 0.85f);
            GuiCards::text(dl, ImVec2(d.x + 30.0f, d.y + 100.0f), IM_COL32(255, 255, 255, 230), s.selling ? "¿Cuántas quieres vender?" : "¿Cuántas quieres comprar?", 0.95f);
            // Selector: -10 -1 [cantidad] +1 +10 MÁX
            const float by = d.y + 130.0f;
            const auto step = [&](const char* id, const char* label, float x, float w, int delta) {
                ImGui::SetCursorScreenPos(ImVec2(d.x + x, by));
                if (GuiCards::button(id, label, ImVec2(w, 40.0f), true, IM_COL32(70, 70, 92, 255))) s.quantity = (std::clamp)(s.quantity + delta, 1, (std::max)(1, limit));
            };
            step("##m10", "-10", 30.0f, 56.0f, -10);
            step("##m1", "-1", 92.0f, 50.0f, -1);
            std::snprintf(line, sizeof(line), "%d", s.quantity);
            dl->AddRectFilled(ImVec2(d.x + 148.0f, by), ImVec2(d.x + 248.0f, by + 40.0f), IM_COL32(12, 12, 20, 255), 12.0f);
            GuiCards::centeredText(dl, ImVec2(d.x + 198.0f, by + 20.0f), IM_COL32(255, 255, 255, 255), line, 1.2f);
            step("##p1", "+1", 254.0f, 50.0f, 1);
            step("##p10", "+10", 310.0f, 56.0f, 10);
            step("##max", "MÁX", 372.0f, 70.0f, limit);
            const int total = s.quantity * unit;
            std::snprintf(line, sizeof(line), "Total: %d pokémonedas", total);
            const bool enough = s.selling || total <= inventory.money();
            GuiCards::text(dl, ImVec2(d.x + 30.0f, d.y + 190.0f), enough ? IM_COL32(255, 215, 90, 255) : IM_COL32(250, 120, 110, 255), line, 1.0f);
            ImGui::SetCursorScreenPos(ImVec2(d.x + 30.0f, d.y + dh - 60.0f));
            const bool confirmed = GuiCards::button("##confirm", s.selling ? "VENDER" : "COMPRAR", ImVec2(220.0f, 42.0f), limit > 0 && enough, s.selling ? IM_COL32(190, 120, 30, 255) : IM_COL32(60, 150, 90, 255)) || (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && limit > 0 && enough);
            ImGui::SetCursorScreenPos(ImVec2(d.x + dw - 30.0f - 220.0f, d.y + dh - 60.0f));
            const bool cancelled = GuiCards::button("##cancel", "CANCELAR", ImVec2(220.0f, 42.0f), true, IM_COL32(70, 70, 92, 255)) || GuiInput::backPressed();
            if (confirmed) {
                if (s.selling) {
                    const int got = inventory.sell(item->id, s.quantity);
                    if (got > 0) say(s, std::to_string(s.quantity) + "x " + data.itemName(*item) + " vendido por " + std::to_string(got), true);
                } else {
                    const std::string result = engine.shopBuy(item->id, s.quantity);
                    say(s, result.empty() ? "No tienes suficiente dinero." : result, !result.empty());
                }
                s.dialogItem = -1;
            } else if (cancelled) s.dialogItem = -1;
            return;
        }
        if (exit || GuiInput::backPressed()) gameState = back;
    }
}
