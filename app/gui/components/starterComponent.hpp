#pragma once
#include <cstdio>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/itemIcon.hpp"
#include "../../models/gameData.hpp"

// Elección del pokémon inicial al empezar una partida nueva: uno de los que marca la base de datos (starter).
// Devuelve el id de la especie elegida cuando el jugador la confirma, o -1 mientras tanto.
namespace StarterComponent {
    namespace detail {
        inline int selected = -1; // id de la especie marcada
    }

    inline int render(const GameData& data) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 190));

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        GuiCards::centeredText(dl, ImVec2(screen.x * 0.5f, 70.0f), IM_COL32(255, 255, 255, 255), "ELIGE A TU POKÉMON INICIAL", 1.8f);

        int count = 0;
        for (const PokemonSpecies& species : data.species) if (species.starter) ++count;
        if (count == 0) return -1;

        constexpr float CARD_W = 250.0f, CARD_H = 300.0f, GAP = 30.0f;
        const float total = static_cast<float>(count) * CARD_W + static_cast<float>(count - 1) * GAP;
        float x = (screen.x - total) * 0.5f;
        const float y = screen.y * 0.5f - CARD_H * 0.5f - 10.0f;

        int confirmed = -1;
        for (const PokemonSpecies& species : data.species) {
            if (!species.starter) continue;
            const ImVec2 a(x, y), b(x + CARD_W, y + CARD_H);
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(species.id);
            const bool clicked = ImGui::InvisibleButton("##starter", ImVec2(CARD_W, CARD_H));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            if (clicked) detail::selected = species.id;

            const bool chosen = detail::selected == species.id;
            const ImU32 tone = species.types.empty() ? GuiStyle::SURFACE : GuiCards::withAlpha(GuiCards::typeColor(species.types.front()), hovered || chosen ? 240 : 190);
            dl->AddRectFilled(a, b, tone, GuiCards::ROUNDING);
            if (chosen) dl->AddRect(a, b, GuiCards::SELECT, GuiCards::ROUNDING, 0, 3.0f);
            ItemIcon::creature(dl, ImVec2(a.x + CARD_W * 0.5f, a.y + 80.0f), 44.0f, species.id);
            GuiCards::centeredText(dl, ImVec2(a.x + CARD_W * 0.5f, a.y + 150.0f), IM_COL32(255, 255, 255, 255), species.name.c_str(), 1.4f);

            float chipsWidth = 0.0f;
            for (const int id : species.types) if (const PokemonType* type = data.type(id)) chipsWidth += GuiCards::textWidth(type->name.c_str(), 0.9f) + 22.0f;
            ImVec2 chipAt(a.x + (CARD_W - chipsWidth) * 0.5f, a.y + 176.0f);
            for (const int id : species.types) if (const PokemonType* type = data.type(id)) chipAt.x += GuiCards::typeChip(dl, chipAt, *type) + 6.0f;

            ImGui::SetCursorScreenPos(ImVec2(a.x + 16.0f, a.y + 212.0f));
            ImGui::PushTextWrapPos(a.x + CARD_W - 16.0f);
            ImGui::TextUnformatted(species.description.c_str());
            ImGui::PopTextWrapPos();
            x += CARD_W + GAP;
        }

        ImGui::SetCursorScreenPos(ImVec2(screen.x * 0.5f - 130.0f, y + CARD_H + 36.0f));
        if (GuiCards::button("##confirmStarter", "ELEGIR", ImVec2(260.0f, 46.0f), detail::selected >= 0, IM_COL32(40, 150, 120, 255))) {
            confirmed = detail::selected;
            detail::selected = -1;
        }
        return confirmed;
    }
}
