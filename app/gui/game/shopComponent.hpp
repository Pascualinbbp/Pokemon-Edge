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

// Tienda del tendero: dos pestañas (COMPRAR / VENDER), una tarjeta por objeto con su icono, precio y unidades que tienes, y botones en cada tarjeta.
// Lo que se vende en la tienda lo decide la base de datos (precio de compra > 0 en el apartado Pokémon); lo que se compra, el precio de venta.
namespace ShopComponent {
    namespace detail {
        struct State {
            bool selling = false;
            std::string message;
            float messageTime = 0.0f;
        };
        inline State& state() { static State s; return s; }
    }

    inline void reset() { detail::state() = {}; }

    inline void render(GameState& gameState, GameState back, GameEngine& engine) {
        detail::State& s = detail::state();
        const GameData& data = engine.data();
        Inventory& inventory = engine.inventory();
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), screen, IM_COL32(0, 0, 0, 160));

        const float width = (std::min)(980.0f, screen.x - 60.0f), height = (std::min)(660.0f, screen.y - 60.0f);
        const ImVec2 origin((screen.x - width) * 0.5f, (screen.y - height) * 0.5f);
        GuiCards::panel(dl, origin, ImVec2(origin.x + width, origin.y + height));
        GuiCards::text(dl, ImVec2(origin.x + 26.0f, origin.y + 18.0f), IM_COL32(255, 255, 255, 255), "Tienda Pokémon", 1.5f);
        char money[48];
        std::snprintf(money, sizeof(money), "%d pokémonedas", inventory.money());
        GuiCards::chip(dl, ImVec2(origin.x + width - 26.0f - GuiCards::textWidth(money, 0.95f) - 20.0f, origin.y + 22.0f), money, IM_COL32(190, 140, 20, 255), 0.95f);

        // Pestañas
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 26.0f, origin.y + 62.0f));
        if (GuiCards::button("##buy", "COMPRAR", ImVec2(150.0f, 34.0f), true, s.selling ? IM_COL32(70, 70, 92, 255) : GuiStyle::ACCENT)) s.selling = false;
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 190.0f, origin.y + 62.0f));
        if (GuiCards::button("##sell", "VENDER", ImVec2(150.0f, 34.0f), true, s.selling ? GuiStyle::ACCENT : IM_COL32(70, 70, 92, 255))) s.selling = true;

        // Lo que se muestra: en venta lo que tiene el tendero; al vender, lo que llevas y tiene precio de venta.
        struct Row { const Item* item; int owned; };
        std::vector<Row> rows;
        if (!s.selling) {
            for (const Item* item : engine.shopStock()) rows.push_back({ item, inventory.count(item->id) });
        } else {
            inventory.forEach([&](const Item& item, int count) { if (item.sellPrice > 0 && count > 0) rows.push_back({ &item, count }); });
        }

        const float listTop = origin.y + 110.0f, listHeight = height - 110.0f - 64.0f;
        constexpr float CARD_H = 86.0f, GAP = 10.0f;
        const float listWidth = width - 52.0f;
        const int columns = listWidth >= 700.0f ? 2 : 1;
        const float cardW = (listWidth - 16.0f - GAP * static_cast<float>(columns - 1)) / static_cast<float>(columns);
        ImGui::SetCursorScreenPos(ImVec2(origin.x + 26.0f, listTop));
        ImGui::BeginChild("##shopList", ImVec2(listWidth, listHeight), false);
        ImDrawList* cl = ImGui::GetWindowDrawList();
        const ImVec2 base = ImGui::GetCursorScreenPos();
        const Item* hoveredItem = nullptr;
        for (size_t i = 0; i < rows.size(); ++i) {
            const Item& item = *rows[i].item;
            const float x = base.x + static_cast<float>(static_cast<int>(i) % columns) * (cardW + GAP);
            const float y = base.y + static_cast<float>(static_cast<int>(i) / columns) * (CARD_H + GAP);
            const ImVec2 a(x, y), b(x + cardW, y + CARD_H);
            ImGui::PushID(static_cast<int>(item.id));
            ImGui::SetCursorScreenPos(a);
            ImGui::InvisibleButton("##card", ImVec2(cardW, CARD_H)); // fondo (solo para detectar el ratón)
            if (ImGui::IsItemHovered()) hoveredItem = &item;
            cl->AddRectFilled(a, b, ImGui::IsItemHovered() ? IM_COL32(60, 60, 84, 255) : GuiStyle::SURFACE, 14.0f);
            ItemIcon::item(cl, ImVec2(x + 40.0f, y + CARD_H * 0.5f), 24.0f, item, data);
            GuiCards::text(cl, ImVec2(x + 80.0f, y + 10.0f), IM_COL32(255, 255, 255, 255), data.itemName(item).c_str(), 1.0f);
            char line[64];
            const int price = s.selling ? item.sellPrice : item.buyPrice;
            std::snprintf(line, sizeof(line), "%d  -  tienes %d", price, rows[i].owned);
            GuiCards::text(cl, ImVec2(x + 80.0f, y + 36.0f), IM_COL32(255, 215, 90, 255), line, 0.85f);
            const float buttonY = y + CARD_H - 34.0f;
            if (!s.selling) {
                const bool one = inventory.money() >= item.buyPrice, five = inventory.money() >= item.buyPrice * 5;
                ImGui::SetCursorScreenPos(ImVec2(b.x - 12.0f - 90.0f - 8.0f - 90.0f, buttonY));
                if (GuiCards::button("##b1", "COMPRAR 1", ImVec2(90.0f, 28.0f), one)) s.message = engine.shopBuy(item.id, 1);
                ImGui::SetCursorScreenPos(ImVec2(b.x - 12.0f - 90.0f, buttonY));
                if (GuiCards::button("##b5", "COMPRAR 5", ImVec2(90.0f, 28.0f), five)) s.message = engine.shopBuy(item.id, 5);
            } else {
                ImGui::SetCursorScreenPos(ImVec2(b.x - 12.0f - 90.0f - 8.0f - 90.0f, buttonY));
                if (GuiCards::button("##s1", "VENDER 1", ImVec2(90.0f, 28.0f), true, IM_COL32(190, 120, 30, 255))) {
                    const int got = inventory.sell(item.id, 1);
                    if (got > 0) s.message = "1x " + data.itemName(item) + " vendido por " + std::to_string(got);
                }
                ImGui::SetCursorScreenPos(ImVec2(b.x - 12.0f - 90.0f, buttonY));
                if (GuiCards::button("##sa", "VENDER TODO", ImVec2(90.0f, 28.0f), true, IM_COL32(190, 120, 30, 255))) {
                    const int units = rows[i].owned, got = inventory.sell(item.id, units);
                    if (got > 0) s.message = std::to_string(units) + "x " + data.itemName(item) + " vendido por " + std::to_string(got);
                }
            }
            ImGui::PopID();
            if (s.message.size() && s.messageTime <= 0.0f) s.messageTime = 2.5f;
        }
        if (rows.empty()) {
            ImGui::SetCursorScreenPos(base);
            ImGui::Dummy(ImVec2(listWidth - 20.0f, 40.0f));
            GuiCards::text(cl, ImVec2(base.x + 8.0f, base.y + 8.0f), GuiStyle::MUTED, s.selling ? "No llevas nada que se pueda vender." : "No hay nada a la venta.", 1.0f);
        } else {
            const int lines = (static_cast<int>(rows.size()) + columns - 1) / columns;
            ImGui::SetCursorScreenPos(ImVec2(base.x, base.y + static_cast<float>(lines) * (CARD_H + GAP)));
            ImGui::Dummy(ImVec2(1.0f, 1.0f)); // el scroll termina siempre en un elemento
        }
        ImGui::EndChild();

        // Pie: descripción del objeto señalado o el resultado de la última operación; ayuda a la izquierda.
        const float footY = origin.y + height - 50.0f;
        if (s.messageTime > 0.0f) {
            s.messageTime -= ImGui::GetIO().DeltaTime;
            if (s.messageTime <= 0.0f) s.message.clear();
        }
        if (!s.message.empty()) GuiCards::text(dl, ImVec2(origin.x + 26.0f, footY), IM_COL32(120, 230, 150, 255), s.message.c_str(), 1.0f);
        else if (hoveredItem) GuiCards::text(dl, ImVec2(origin.x + 26.0f, footY), IM_COL32(255, 255, 255, 220), hoveredItem->description.c_str(), 0.9f);
        else GuiCards::text(dl, ImVec2(origin.x + 26.0f, footY), GuiStyle::MUTED, "Pulsa los botones de cada objeto   ESC: salir", 0.9f);

        ImGui::SetCursorScreenPos(ImVec2(origin.x + width - 26.0f - 120.0f, origin.y + height - 52.0f));
        if (GuiCards::button("##exit", "SALIR", ImVec2(120.0f, 34.0f), true, IM_COL32(70, 70, 92, 255)) || GuiInput::backPressed()) gameState = back;
    }
}
