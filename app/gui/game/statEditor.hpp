#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include "imgui.h"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/rules/ivRules.hpp"
#include "../../engine/world/rules/statRules.hpp"
#include "../../engine/world/state/pokemonStorage.hpp"

// Entrenamiento de un pokémon desde su ficha: reparte sus EVs (510 en total, 252 como máximo en una estadística) y elige su naturaleza cuando quiera.
// Los IV (innatos, con su potencial) solo se ven aquí: suben con vitaminas y chapas desde la mochila. Es su propia ventana a pantalla completa y siempre
// la de más arriba. Devuelve false cuando se cierra.
namespace StatEditor {
    namespace detail {
        inline constexpr int ORDER[6] = { 0, 1, 3, 2, 4, 5 }; // PS, ataque, defensa, ataque esp., defensa esp., velocidad
        inline constexpr int STEP = 4;                       // 4 EV = 1 punto de estadística
    }

    // Contenido del entrenamiento dentro de un rectángulo ('o', ancho W, alto H); todo se ajusta al espacio que hay. 'embedded' = va dentro de una pestaña de la ficha:
    // sin marco, título ni botón de cerrar. Los EVs se reparten arrastrando la barra (a saltos de 4, cada salto marcado); la naturaleza se elige en la cuadrícula.
    inline void content(ImDrawList* dl, const GameData& data, PokemonStorage& storage, bool inTeam, int index, const OwnedPokemon& owned, const PokemonSpecies* species, const ImVec2& o, float W, float H, bool embedded) {
        constexpr float PAD = 18.0f;
        if (!embedded) GuiCards::panel(dl, o, ImVec2(o.x + W, o.y + H));
        const Nature* nature = data.nature(owned.nature);
        char text[128];
        float top = o.y + (embedded ? 4.0f : 14.0f);
        if (!embedded) {
            std::snprintf(text, sizeof(text), "Entrenamiento de %s  (Nv. %d)", species->name.c_str(), owned.level);
            GuiCards::text(dl, ImVec2(o.x + PAD, top), IM_COL32(255, 255, 255, 255), text, 1.3f);
            top += 36.0f;
        }
        const float bottom = o.y + H - (embedded ? 8.0f : 56.0f);
        const int left = StatRules::remaining(owned.evs);
        std::snprintf(text, sizeof(text), "EVs: %d / %d repartidos  ·  quedan %d  ·  máximo %d por estadística  ·  arrastra las barras", StatRules::total(owned.evs), StatRules::TOTAL_EV, left, StatRules::MAX_EV);
        GuiCards::text(dl, ImVec2(o.x + PAD, top), left > 0 ? IM_COL32(255, 200, 90, 255) : GuiStyle::MUTED, text, 0.85f);
        top += 24.0f;

        // --- EVs por estadística (izquierda) ---
        const float colW = (W - 3.0f * PAD) * 0.54f;
        const float footer = 38.0f; // botón de reiniciar
        const float rowGap = 5.0f;
        const float rowH = (std::clamp)((bottom - top - footer - 6.0f * rowGap) / 6.0f, 26.0f, 46.0f);
        const float scale = (std::clamp)(rowH / 44.0f, 0.7f, 1.0f);
        for (int row = 0; row < 6; ++row) {
            const int stat = detail::ORDER[row];
            const float rowY = top + static_cast<float>(row) * (rowH + rowGap);
            const float cy = rowY + rowH * 0.5f;
            dl->AddRectFilled(ImVec2(o.x + PAD, rowY), ImVec2(o.x + PAD + colW, rowY + rowH), IM_COL32(40, 40, 52, 200), 10.0f);
            const int percent = StatRules::naturePercent(nature, stat);
            const ImU32 nameColor = percent > 100 ? IM_COL32(255, 150, 120, 255) : percent < 100 ? IM_COL32(130, 170, 255, 255) : IM_COL32(255, 255, 255, 240);
            std::snprintf(text, sizeof(text), "%s%s", BaseStats::name(stat), percent > 100 ? " +" : percent < 100 ? " -" : "");
            GuiCards::centeredTextLeft(dl, ImVec2(o.x + PAD + 10.0f, cy), nameColor, text, 0.88f * scale);
            if (owned.analyzed) std::snprintf(text, sizeof(text), "%d  ·  IV %d/%d", species->stats.at(stat), owned.ivs[static_cast<size_t>(stat)], owned.ivCaps[static_cast<size_t>(stat)]);
            else std::snprintf(text, sizeof(text), "%d  ·  IV ?/?", species->stats.at(stat)); // los IV no se ven hasta analizar al pokémon en la máquina
            GuiCards::centeredTextLeft(dl, ImVec2(o.x + PAD + 108.0f, cy), IM_COL32(200, 200, 215, 255), text, 0.8f * scale);

            // Barra de EV al estilo Champions: casillas de 4 EV marcadas; clic o arrastre para fijar el valor.
            const float barX = o.x + PAD + 262.0f, barW = (std::max)(60.0f, colW - 262.0f - 64.0f), barH = (std::min)(20.0f, rowH - 12.0f);
            const float by = cy - barH * 0.5f;
            const int ev = owned.evs[static_cast<size_t>(stat)];
            const int cells = StatRules::MAX_EV / detail::STEP;
            const float cellW = barW / static_cast<float>(cells);
            const int filled = ev / detail::STEP;
            dl->AddRectFilled(ImVec2(barX, by), ImVec2(barX + barW, by + barH), IM_COL32(0, 0, 0, 150), 6.0f);
            if (filled > 0) dl->AddRectFilled(ImVec2(barX, by), ImVec2(barX + cellW * static_cast<float>(filled), by + barH), IM_COL32(255, 160, 40, 255), 6.0f, filled >= cells ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
            for (int c = 1; c < cells; ++c) {
                const float x = barX + cellW * static_cast<float>(c);
                const bool major = c % 8 == 0;
                dl->AddLine(ImVec2(x, by + (major ? 0.0f : barH * 0.55f)), ImVec2(x, by + barH), major ? IM_COL32(0, 0, 0, 190) : IM_COL32(0, 0, 0, 95), 1.0f);
            }
            dl->AddRect(ImVec2(barX, by), ImVec2(barX + barW, by + barH), IM_COL32(150, 155, 180, 200), 6.0f, 0, 1.5f);
            ImGui::SetCursorScreenPos(ImVec2(barX - 4.0f, by - 4.0f));
            ImGui::PushID(100 + stat);
            ImGui::InvisibleButton("##evBar", ImVec2(barW + 8.0f, barH + 8.0f));
            if (ImGui::IsItemActive()) {
                const float fraction = (std::clamp)((ImGui::GetIO().MousePos.x - barX) / barW, 0.0f, 1.0f);
                const int wanted = static_cast<int>(std::lround(fraction * static_cast<float>(cells))) * detail::STEP;
                if (wanted != ev) storage.setEv(inTeam, index, stat, wanted);
            }
            ImGui::PopID();
            std::snprintf(text, sizeof(text), "%d", ev);
            GuiCards::centeredText(dl, ImVec2(barX + barW * 0.5f, cy), IM_COL32(255, 255, 255, 255), text, 0.72f * scale);

            const int value = stat == 0 ? storage.maxHp(owned) : storage.statOf(owned, stat);
            std::snprintf(text, sizeof(text), "%d", value);
            GuiCards::centeredText(dl, ImVec2(o.x + PAD + colW - 28.0f, cy), IM_COL32(120, 230, 150, 255), text, 1.0f * scale);
        }
        const float resetY = top + 6.0f * (rowH + rowGap) + 4.0f;
        ImGui::SetCursorScreenPos(ImVec2(o.x + PAD, resetY));
        if (GuiCards::button("##resetEv", "REINICIAR EVs", ImVec2(180.0f, 32.0f), StatRules::total(owned.evs) > 0, IM_COL32(150, 70, 70, 255))) storage.resetEvs(inTeam, index);

        // --- Naturaleza (derecha): cuadrícula que se ajusta al hueco ---
        const float nx = o.x + 2.0f * PAD + colW, nw = o.x + W - PAD - nx;
        GuiCards::text(dl, ImVec2(nx, top - 24.0f), GuiStyle::MUTED, "NATURALEZA  (sube un 10 % una estadística y baja otra)", 0.78f);
        const std::vector<Nature>& natures = data.natures;
        const int count = static_cast<int>(natures.size());
        if (count <= 0) return;
        const float gap = 6.0f, gridTop = top, gridH = bottom - top;
        int perRow = 5, rows = (count + perRow - 1) / perRow;
        float cw = (nw - gap * static_cast<float>(perRow - 1)) / static_cast<float>(perRow);
        float ch = (gridH - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
        if (ch < 34.0f) { perRow = 7; rows = (count + perRow - 1) / perRow; cw = (nw - gap * static_cast<float>(perRow - 1)) / static_cast<float>(perRow); ch = (gridH - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows); }
        ch = (std::min)(ch, 62.0f);
        const float ns = (std::clamp)(ch / 50.0f, 0.62f, 1.0f);
        for (int i = 0; i < count; ++i) {
            const Nature& n = natures[static_cast<size_t>(i)];
            const float cx = nx + static_cast<float>(i % perRow) * (cw + gap);
            const float cy = gridTop + static_cast<float>(i / perRow) * (ch + gap);
            ImGui::SetCursorScreenPos(ImVec2(cx, cy));
            ImGui::PushID(300 + n.id);
            const bool clicked = ImGui::InvisibleButton("##nature", ImVec2(cw, ch));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            const bool selected = owned.nature == n.id;
            dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + ch), selected ? IM_COL32(70, 110, 190, 235) : hovered ? IM_COL32(64, 64, 84, 235) : IM_COL32(40, 40, 52, 210), 10.0f);
            if (selected) dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cw, cy + ch), IM_COL32(255, 215, 90, 255), 10.0f, 0, 2.5f);
            GuiCards::centeredText(dl, ImVec2(cx + cw * 0.5f, cy + ch * 0.3f), IM_COL32(255, 255, 255, 255), n.name.c_str(), 0.85f * ns);
            if (n.neutral()) GuiCards::centeredText(dl, ImVec2(cx + cw * 0.5f, cy + ch * 0.7f), GuiStyle::MUTED, "neutra", 0.68f * ns);
            else {
                std::snprintf(text, sizeof(text), "+%s", BaseStats::name(n.increased));
                GuiCards::centeredText(dl, ImVec2(cx + cw * 0.27f, cy + ch * 0.72f), IM_COL32(255, 150, 120, 255), text, 0.62f * ns);
                std::snprintf(text, sizeof(text), "-%s", BaseStats::name(n.decreased));
                GuiCards::centeredText(dl, ImVec2(cx + cw * 0.73f, cy + ch * 0.72f), IM_COL32(130, 170, 255, 255), text, 0.62f * ns);
            }
            if (clicked) storage.setNature(inTeam, index, n.id);
        }
    }

    inline bool render(const GameData& data, PokemonStorage& storage, bool inTeam, int index) {
        const std::vector<OwnedPokemon>& list = inTeam ? storage.team() : storage.pc();
        if (index < 0 || index >= static_cast<int>(list.size())) return false;
        const OwnedPokemon owned = list[static_cast<size_t>(index)];
        const PokemonSpecies* species = data.speciesById(owned.speciesId);
        if (!species) return false;
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(screen);
        ImGui::SetNextWindowFocus();
        ImGui::Begin("##statEditor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), screen, IM_COL32(0, 0, 0, 170));
        const float W = (std::min)(screen.x - 40.0f, 1180.0f), H = (std::min)(screen.y - 40.0f, 700.0f);
        const ImVec2 o((screen.x - W) * 0.5f, (screen.y - H) * 0.5f);
        content(dl, data, storage, inTeam, index, owned, species, o, W, H, false);
        bool open = true;
        ImGui::SetCursorScreenPos(ImVec2(o.x + W - 22.0f - 140.0f, o.y + H - 50.0f));
        if (GuiCards::button("##closeStats", "CERRAR", ImVec2(140.0f, 36.0f), true, IM_COL32(70, 70, 92, 255)) || GuiInput::backPressed()) open = false;
        ImGui::End();
        return open;
    }
}
