#pragma once
#include <cfloat>
#include <cstdio>
#include "imgui.h"
#include "../guiCards.hpp"
#include "itemIcon.hpp"
#include "typeIcons.hpp"
#include "../../../engine/world/entities/pokeball.hpp"
#include "../../../models/gameData.hpp"

// Casilla circular de un pokémon del equipo (la del juego y la de la gestión de pokémon): su icono sobre el color de su
// tipo, "Nivel x" arriba en el centro, sus tipos abajo a la izquierda, su pokéball abajo a la derecha y, si se pide, su número.
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

        // "Nivel x": etiqueta pequeña sobre el borde superior, oscura y con el filo del color del tipo.
        char text[24];
        std::snprintf(text, sizeof(text), "Nivel %d", level);
        ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize() * 0.68f;
        const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        const float w = size.x + radius * 0.4f, h = size.y + 3.0f;
        const ImVec2 pill(c.x - w * 0.5f, c.y - radius - h * 0.5f);
        dl->AddRectFilled(pill, ImVec2(pill.x + w, pill.y + h), fade(IM_COL32(14, 16, 26, 225)), h * 0.5f);
        dl->AddRect(pill, ImVec2(pill.x + w, pill.y + h), fade(GuiCards::withAlpha(tone, 210)), h * 0.5f, 0, 1.0f);
        dl->AddText(font, fontSize, ImVec2(c.x - size.x * 0.5f, pill.y + (h - size.y) * 0.5f), fade(IM_COL32(240, 242, 250, 255)), text);

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
        const ImVec2 badge(c.x - radius * 0.86f, c.y - radius * 0.3f);
        dl->AddCircleFilled(badge, 9.0f, fade(IM_COL32(20, 20, 30, 230)), 16);
        dl->AddText(ImVec2(badge.x - ImGui::CalcTextSize(text).x * 0.5f, badge.y - ImGui::GetFontSize() * 0.5f), fade(IM_COL32(255, 255, 255, 255)), text);
    }

    // Hueco vacío del equipo.
    inline void empty(ImDrawList* dl, const ImVec2& c, float radius, bool highlighted = false) {
        dl->AddCircleFilled(c, radius, IM_COL32(255, 255, 255, highlighted ? 40 : 14), 40);
        dl->AddCircle(c, radius, IM_COL32(255, 255, 255, highlighted ? 200 : 55), 40, highlighted ? 3.0f : 1.5f);
    }
}
