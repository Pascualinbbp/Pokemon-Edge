#pragma once
#include <cstdio>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/core/gameEngine.hpp"

// Elección de semillas al interactuar con una parcela: una tarjeta por cada tipo que se tiene (icono, nombre y cantidad); un clic siembra.
// ESC o fuera de las tarjetas cancela. Cualquier cultivo vale para cualquier parcela.
namespace SeedComponent {
    inline void render(GameState& state, GameState back, GameEngine& engine) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 150));
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const std::vector<Scene::SeedChoice> choices = engine.seedChoices();

        constexpr float CARD_W = 190.0f, CARD_H = 210.0f, GAP = 18.0f, PAD = 26.0f;
        const float count = static_cast<float>(choices.size());
        const float width = (std::max)(count, 1.0f) * CARD_W + ((std::max)(count, 1.0f) - 1.0f) * GAP + PAD * 2.0f;
        const float height = CARD_H + PAD * 2.0f + 46.0f;
        const ImVec2 origin((screen.x - width) * 0.5f, (screen.y - height) * 0.5f);
        GuiCards::panel(dl, origin, ImVec2(origin.x + width, origin.y + height));
        GuiCards::centeredText(dl, ImVec2(origin.x + width * 0.5f, origin.y + PAD + 6.0f), IM_COL32(255, 255, 255, 255), "¿Qué quieres plantar?", 1.3f);

        if (choices.empty()) { state = back; return; } // se quedó sin semillas
        float x = origin.x + PAD;
        const float y = origin.y + PAD + 40.0f;
        for (size_t i = 0; i < choices.size(); ++i) {
            const Scene::SeedChoice& choice = choices[i];
            ImGui::SetCursorScreenPos(ImVec2(x, y));
            ImGui::PushID(static_cast<int>(i));
            const bool clicked = ImGui::InvisibleButton("##seed", ImVec2(CARD_W, CARD_H));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + CARD_W, y + CARD_H), hovered ? IM_COL32(70, 70, 96, 255) : GuiStyle::SURFACE, 14.0f);
            if (hovered) dl->AddRect(ImVec2(x - 2.0f, y - 2.0f), ImVec2(x + CARD_W + 2.0f, y + CARD_H + 2.0f), GuiCards::SELECT, 16.0f, 0, 2.0f);
            if (choice.item) ItemIcon::item(dl, ImVec2(x + CARD_W * 0.5f, y + 70.0f), 42.0f, *choice.item, engine.data());
            GuiCards::centeredText(dl, ImVec2(x + CARD_W * 0.5f, y + 140.0f), IM_COL32(255, 255, 255, 255), choice.name.c_str(), 0.95f);
            char amount[24];
            std::snprintf(amount, sizeof(amount), "x%d", choice.count);
            GuiCards::centeredText(dl, ImVec2(x + CARD_W * 0.5f, y + 172.0f), IM_COL32(255, 215, 90, 255), amount, 1.1f);
            if (clicked) { engine.plantSeed(choice.crop); state = back; return; }
            x += CARD_W + GAP;
        }
        GuiCards::text(dl, ImVec2(origin.x + PAD, origin.y + height + 10.0f), GuiStyle::MUTED, "Clic: plantar   ESC: cancelar", 0.9f);
        if (GuiInput::backPressed()) state = back;
    }
}
