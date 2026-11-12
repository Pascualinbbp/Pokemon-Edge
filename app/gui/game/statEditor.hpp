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

        constexpr float PAD = 22.0f, ROW_H = 44.0f, NATURE_W = 150.0f, NATURE_H = 50.0f;
        const float W = (std::min)(screen.x - 40.0f, 1180.0f), H = (std::min)(screen.y - 40.0f, 700.0f);
        const ImVec2 o((screen.x - W) * 0.5f, (screen.y - H) * 0.5f);
        GuiCards::panel(dl, o, ImVec2(o.x + W, o.y + H));

        const Nature* nature = data.nature(owned.nature);
        char text[128];
        std::snprintf(text, sizeof(text), "Entrenamiento de %s  (Nv. %d)", species->name.c_str(), owned.level);
        GuiCards::text(dl, ImVec2(o.x + PAD, o.y + 14.0f), IM_COL32(255, 255, 255, 255), text, 1.3f);
        const int left = StatRules::remaining(owned.evs);
        std::snprintf(text, sizeof(text), "EVs: %d / %d repartidos   ·   quedan %d   ·   máximo %d por estadística", StatRules::total(owned.evs), StatRules::TOTAL_EV, left, StatRules::MAX_EV);
        GuiCards::text(dl, ImVec2(o.x + PAD, o.y + 50.0f), left > 0 ? IM_COL32(255, 200, 90, 255) : GuiStyle::MUTED, text, 0.9f);

        // --- EVs por estadística (izquierda) ---
        const float colW = (W - 3.0f * PAD) * 0.52f;
        float y = o.y + 84.0f;
        GuiCards::text(dl, ImVec2(o.x + PAD, y), GuiStyle::MUTED, "ESTADÍSTICA", 0.75f);
        GuiCards::text(dl, ImVec2(o.x + PAD + 112.0f, y), GuiStyle::MUTED, "BASE  IV", 0.75f);
        GuiCards::text(dl, ImVec2(o.x + PAD + colW - 74.0f, y), GuiStyle::MUTED, "TOTAL", 0.75f);
        y += 22.0f;
        for (int row = 0; row < 6; ++row) {
            const int stat = detail::ORDER[row];
            const float rowY = y + static_cast<float>(row) * (ROW_H + 6.0f);
            dl->AddRectFilled(ImVec2(o.x + PAD, rowY), ImVec2(o.x + PAD + colW, rowY + ROW_H), IM_COL32(40, 40, 52, 200), 10.0f);
            const int percent = StatRules::naturePercent(nature, stat);
            const ImU32 nameColor = percent > 100 ? IM_COL32(255, 150, 120, 255) : percent < 100 ? IM_COL32(130, 170, 255, 255) : IM_COL32(255, 255, 255, 240);
            std::snprintf(text, sizeof(text), "%s%s", BaseStats::name(stat), percent > 100 ? " +" : percent < 100 ? " -" : "");
            GuiCards::text(dl, ImVec2(o.x + PAD + 10.0f, rowY + 13.0f), nameColor, text, 0.9f);
            if (owned.analyzed) std::snprintf(text, sizeof(text), "%d   %d/%d", species->stats.at(stat), owned.ivs[static_cast<size_t>(stat)], owned.ivCaps[static_cast<size_t>(stat)]);
            else std::snprintf(text, sizeof(text), "%d   ?/?", species->stats.at(stat)); // los IV no se ven hasta analizar al pokémon en la máquina
            GuiCards::text(dl, ImVec2(o.x + PAD + 112.0f, rowY + 13.0f), IM_COL32(200, 200, 215, 255), text, 0.85f);

            // Barra de EV: clic o arrastre para fijar el valor (de 4 en 4).
            const float barX = o.x + PAD + 214.0f, barW = colW - 214.0f - 150.0f - 74.0f;
            const int ev = owned.evs[static_cast<size_t>(stat)];
            dl->AddRectFilled(ImVec2(barX, rowY + 14.0f), ImVec2(barX + barW, rowY + 30.0f), IM_COL32(0, 0, 0, 140), 8.0f);
            if (ev > 0) dl->AddRectFilled(ImVec2(barX, rowY + 14.0f), ImVec2(barX + barW * static_cast<float>(ev) / static_cast<float>(StatRules::MAX_EV), rowY + 30.0f), IM_COL32(255, 160, 40, 255), 8.0f);
            ImGui::SetCursorScreenPos(ImVec2(barX, rowY + 6.0f));
            ImGui::PushID(100 + stat);
            ImGui::InvisibleButton("##bar", ImVec2(barW, ROW_H - 12.0f));
            if (ImGui::IsItemActive()) {
                const float t = (std::clamp)((ImGui::GetIO().MousePos.x - barX) / barW, 0.0f, 1.0f);
                const int wanted = static_cast<int>(t * static_cast<float>(StatRules::MAX_EV) / static_cast<float>(detail::STEP) + 0.5f) * detail::STEP;
                if (wanted != ev) storage.setEv(inTeam, index, stat, wanted);
            }
            ImGui::PopID();
            std::snprintf(text, sizeof(text), "%d", ev);
            GuiCards::centeredText(dl, ImVec2(barX + barW * 0.5f, rowY + 22.0f), IM_COL32(255, 255, 255, 255), text, 0.8f);

            // -, +, MÁX
            const float bx = barX + barW + 8.0f;
            ImGui::PushID(200 + stat);
            ImGui::SetCursorScreenPos(ImVec2(bx, rowY + 7.0f));
            if (GuiCards::button("##less", "-", ImVec2(34.0f, 30.0f), ev > 0, IM_COL32(70, 70, 92, 255))) storage.setEv(inTeam, index, stat, ev - detail::STEP);
            ImGui::SetCursorScreenPos(ImVec2(bx + 40.0f, rowY + 7.0f));
            if (GuiCards::button("##more", "+", ImVec2(34.0f, 30.0f), ev < StatRules::MAX_EV && left > 0, IM_COL32(70, 70, 92, 255))) storage.setEv(inTeam, index, stat, ev + detail::STEP);
            ImGui::SetCursorScreenPos(ImVec2(bx + 80.0f, rowY + 7.0f));
            if (GuiCards::button("##max", "MÁX", ImVec2(60.0f, 30.0f), ev < StatRules::MAX_EV && left > 0, IM_COL32(200, 120, 40, 255))) storage.setEv(inTeam, index, stat, StatRules::MAX_EV);
            ImGui::PopID();

            const int value = stat == 0 ? storage.maxHp(owned) : storage.statOf(owned, stat);
            std::snprintf(text, sizeof(text), "%d", value);
            GuiCards::text(dl, ImVec2(o.x + PAD + colW - 62.0f, rowY + 11.0f), IM_COL32(120, 230, 150, 255), text, 1.05f);
        }
        const float resetY = y + 6.0f * (ROW_H + 6.0f) + 6.0f;
        ImGui::SetCursorScreenPos(ImVec2(o.x + PAD, resetY));
        if (GuiCards::button("##resetEv", "REINICIAR EVs", ImVec2(180.0f, 34.0f), StatRules::total(owned.evs) > 0, IM_COL32(150, 70, 70, 255))) storage.resetEvs(inTeam, index);

        // --- Naturaleza (derecha): cuadrícula 5x5 ---
        const float nx = o.x + 2.0f * PAD + colW;
        GuiCards::text(dl, ImVec2(nx, o.y + 84.0f), GuiStyle::MUTED, "NATURALEZA (sube un 10 % una estadística y baja otra; clic para cambiarla)", 0.75f);
        const std::vector<Nature>& natures = data.natures;
        const int perRow = (std::max)(1, static_cast<int>((W - PAD - (nx - o.x)) / (NATURE_W + 8.0f)));
        for (size_t i = 0; i < natures.size(); ++i) {
            const Nature& n = natures[i];
            const float cx = nx + static_cast<float>(static_cast<int>(i) % perRow) * (NATURE_W + 8.0f);
            const float cy = o.y + 108.0f + static_cast<float>(static_cast<int>(i) / perRow) * (NATURE_H + 8.0f);
            if (cy + NATURE_H > o.y + H - 60.0f) break;
            ImGui::SetCursorScreenPos(ImVec2(cx, cy));
            ImGui::PushID(300 + n.id);
            const bool clicked = ImGui::InvisibleButton("##nature", ImVec2(NATURE_W, NATURE_H));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            const bool selected = owned.nature == n.id;
            dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + NATURE_W, cy + NATURE_H), selected ? IM_COL32(70, 110, 190, 235) : hovered ? IM_COL32(64, 64, 84, 235) : IM_COL32(40, 40, 52, 210), 10.0f);
            if (selected) dl->AddRect(ImVec2(cx, cy), ImVec2(cx + NATURE_W, cy + NATURE_H), IM_COL32(255, 215, 90, 255), 10.0f, 0, 2.5f);
            GuiCards::text(dl, ImVec2(cx + 8.0f, cy + 5.0f), IM_COL32(255, 255, 255, 255), n.name.c_str(), 0.9f);
            if (n.neutral()) GuiCards::text(dl, ImVec2(cx + 8.0f, cy + 27.0f), GuiStyle::MUTED, "neutra", 0.72f);
            else {
                std::snprintf(text, sizeof(text), "+%s", BaseStats::name(n.increased));
                GuiCards::text(dl, ImVec2(cx + 8.0f, cy + 22.0f), IM_COL32(255, 150, 120, 255), text, 0.68f);
                std::snprintf(text, sizeof(text), "-%s", BaseStats::name(n.decreased));
                GuiCards::text(dl, ImVec2(cx + 8.0f, cy + 34.0f), IM_COL32(130, 170, 255, 255), text, 0.68f);
            }
            if (clicked) storage.setNature(inTeam, index, n.id);
        }

        bool open = true;
        ImGui::SetCursorScreenPos(ImVec2(o.x + W - PAD - 140.0f, o.y + H - 50.0f));
        if (GuiCards::button("##closeStats", "CERRAR", ImVec2(140.0f, 36.0f), true, IM_COL32(70, 70, 92, 255)) || GuiInput::backPressed()) open = false;
        ImGui::End();
        return open;
    }
}
