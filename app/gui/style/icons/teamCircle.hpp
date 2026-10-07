#pragma once
#include <cstdio>
#include "imgui.h"
#include "../guiCards.hpp"
#include "itemIcon.hpp"
#include "typeIcons.hpp"
#include "../../../engine/world/entities/pokeball.hpp"
#include "../../../models/gameData.hpp"

// Casilla circular de un pokémon del equipo (la del juego y la de la gestión de pokémon): su icono sobre el color de su
// tipo, el nivel abajo, sus tipos abajo a la izquierda, su pokéball abajo a la derecha y, si se pide, su número arriba.
namespace TeamCircle {
    // 'emphasis' resalta el pokémon que acompaña al jugador.
    inline void draw(ImDrawList* dl, const ImVec2& c, float radius, const GameData& data, const PokemonSpecies& species, int level,
                     bool shiny, int ballId, bool emphasis, int number = 0, float alpha = 1.0f) {
        const float mini = radius * 0.3f;
        const auto fade = [&](ImU32 color) { return GuiCards::withAlpha(color, static_cast<int>(((color >> 24) & 0xFF) * alpha)); };
        const ImU32 tone = species.types.empty() ? IM_COL32(60, 60, 80, 255) : GuiCards::typeColor(species.types.front());
        dl->AddCircleFilled(c, radius, fade(GuiCards::withAlpha(tone, emphasis ? 235 : 150)), 40);
        dl->AddCircle(c, radius, fade(shiny ? IM_COL32(255, 220, 90, 255) : IM_COL32(255, 255, 255, emphasis ? 255 : 90)), 40, emphasis ? 3.5f : 1.5f);
        ItemIcon::creature(dl, ImVec2(c.x, c.y - radius * 0.13f), radius * 0.5f, species.id, (emphasis ? 1.0f : 0.8f) * alpha);

        char text[16];
        std::snprintf(text, sizeof(text), "%d", level);
        const float w = ImGui::CalcTextSize(text).x + 14.0f;
        const ImVec2 pill(c.x - w * 0.5f, c.y + radius - 11.0f);
        dl->AddRectFilled(pill, ImVec2(pill.x + w, pill.y + 18.0f), fade(IM_COL32(20, 20, 30, 230)), 9.0f);
        dl->AddText(ImVec2(pill.x + 7.0f, pill.y + 2.0f), fade(IM_COL32(255, 255, 255, 255)), text);

        float typeX = c.x - radius * 0.75f;
        for (const int id : species.types) {
            if (const PokemonType* type = data.type(id)) TypeIcons::draw(dl, ImVec2(typeX, c.y + radius * 0.7f), mini, *type);
            typeX += mini * 1.5f;
        }
        if (data.ball(ballId)) {
            dl->AddCircleFilled(ImVec2(c.x + radius * 0.75f, c.y + radius * 0.7f), mini + 2.0f, fade(IM_COL32(20, 20, 30, 220)), 20);
            ItemIcon::ball(dl, ImVec2(c.x + radius * 0.75f, c.y + radius * 0.7f), mini, PokeballStyle::color(ballId));
        }

        if (number <= 0) return;
        std::snprintf(text, sizeof(text), "%d", number);
        const ImVec2 badge(c.x + radius * 0.72f, c.y - radius * 0.72f);
        dl->AddCircleFilled(badge, 9.0f, fade(IM_COL32(20, 20, 30, 230)), 16);
        dl->AddText(ImVec2(badge.x - ImGui::CalcTextSize(text).x * 0.5f, badge.y - ImGui::GetFontSize() * 0.5f), fade(IM_COL32(255, 255, 255, 255)), text);
    }

    // Hueco vacío del equipo.
    inline void empty(ImDrawList* dl, const ImVec2& c, float radius, bool highlighted = false) {
        dl->AddCircleFilled(c, radius, IM_COL32(255, 255, 255, highlighted ? 40 : 14), 40);
        dl->AddCircle(c, radius, IM_COL32(255, 255, 255, highlighted ? 200 : 55), 40, highlighted ? 3.0f : 1.5f);
    }
}
