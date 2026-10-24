#pragma once
#include <algorithm>
#include <cstdio>
#include <random>
#include <map>
#include <set>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../../models/gameData.hpp"

// Elección del pokémon inicial al empezar una partida nueva: uno de los que marca la base de datos (starter).
// Pasado un tiempo se va retirando uno al azar (uno cada DROP_INTERVAL s, sean 3 o 15; sin barras ni avisos); si se acaban, se ofrecen Pichu y Eevee.
// Devuelve el id de la especie elegida cuando el jugador la confirma, o -1 mientras tanto.
namespace StarterComponent {
    namespace detail {
        inline int selected = -1; // id de la especie marcada
        inline constexpr double DROP_INTERVAL = 8.0;          // segundos entre descartes
        inline constexpr const char* FALLBACK[] = { "Pichu", "Eevee" };
        inline std::set<int> discarded;                       // ids descartados por el tiempo
        inline double nextDrop = -1.0;                        // instante del próximo descarte (-1 = aún sin iniciar)

        inline void reset() { selected = -1; discarded.clear(); nextDrop = -1.0; }

        // Las especies que se pueden elegir ahora; 'fallback' = se acabaron las iniciales y se ofrecen las de reserva.
        inline std::vector<const PokemonSpecies*> offered(const GameData& data, bool& fallback) {
            std::vector<const PokemonSpecies*> out;
            for (const PokemonSpecies& species : data.species) if (species.starter && !discarded.count(species.id)) out.push_back(&species);
            fallback = out.empty();
            if (fallback) {
                for (const char* name : FALLBACK) for (const PokemonSpecies& species : data.species) if (species.name == name) { out.push_back(&species); break; }
                if (out.empty()) for (const PokemonSpecies& species : data.species) if (species.starter) { out.push_back(&species); break; }
            }
            return out;
        }
    }

    inline int render(const GameData& data) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 190));

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        GuiCards::centeredText(dl, ImVec2(screen.x * 0.5f, 70.0f), IM_COL32(255, 255, 255, 255), "ELIGE A TU POKÉMON INICIAL", 1.8f);

        const double now = ImGui::GetTime();
        if (detail::nextDrop < 0.0) detail::nextDrop = now + detail::DROP_INTERVAL;
        bool fallback = false;
        std::vector<const PokemonSpecies*> offered = detail::offered(data, fallback);
        while (!fallback && now >= detail::nextDrop) { // cada DROP_INTERVAL se retira uno al azar, aunque la ventana se haya quedado sin dibujar
            static std::mt19937 random{ std::random_device{}() };
            const PokemonSpecies* gone = offered[std::uniform_int_distribution<size_t>(0, offered.size() - 1)(random)];
            if (detail::selected == gone->id) detail::selected = -1;
            detail::discarded.insert(gone->id);
            detail::nextDrop += detail::DROP_INTERVAL;
            offered = detail::offered(data, fallback);
        }
        const int count = static_cast<int>(offered.size());
        if (count == 0) return -1;
        if (detail::selected >= 0 && std::none_of(offered.begin(), offered.end(), [](const PokemonSpecies* sp) { return sp->id == detail::selected; })) detail::selected = -1;
        if (fallback) GuiCards::centeredText(dl, ImVec2(screen.x * 0.5f, 110.0f), IM_COL32(255, 215, 90, 255), "Ninguno de los iniciales ha esperado: elige uno de reserva", 1.0f);

        constexpr float CARD_W = 250.0f, CARD_H = 300.0f, GAP = 30.0f;
        const float total = static_cast<float>(count) * CARD_W + static_cast<float>(count - 1) * GAP;
        float x = (screen.x - total) * 0.5f;
        const float y = screen.y * 0.5f - CARD_H * 0.5f - 10.0f;

        int confirmed = -1;
        for (const PokemonSpecies* choice : offered) {
            const PokemonSpecies& species = *choice;
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
            detail::reset();
        }
        return confirmed;
    }
}
