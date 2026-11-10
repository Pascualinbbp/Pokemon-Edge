#pragma once
#include <cstdio>
#include <string>
#include "imgui.h"
#include "learnComponent.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/state/pokemonStorage.hpp"

// Edición de los movimientos de un pokémon desde el menú de pokémon: a la izquierda los 4 que lleva (clic para elegir el hueco) y a la derecha los que tiene
// desbloqueados (los que intentó aprender); un clic en uno lo pone en el hueco elegido. Devuelve false cuando se cierra.
namespace MoveEditor {
    namespace detail {
        inline int slot = 0;
        inline float scroll = 0.0f;
    }

    inline void reset() { detail::slot = 0; detail::scroll = 0.0f; }

    inline bool render(const GameData& data, PokemonStorage& storage, bool inTeam, int index) {
        const std::vector<OwnedPokemon>& list = inTeam ? storage.team() : storage.pc();
        if (index < 0 || index >= static_cast<int>(list.size())) return false;
        const OwnedPokemon& owned = list[static_cast<size_t>(index)];
        const PokemonSpecies* species = data.speciesById(owned.speciesId);
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        // El editor es su propia ventana a pantalla completa y siempre la de más arriba: nada del resto de la interfaz se dibuja encima ni recibe clics.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(screen);
        ImGui::SetNextWindowFocus();
        ImGui::Begin("##moveEditor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), screen, IM_COL32(0, 0, 0, 170));
        constexpr float W = 1040.0f, PAD = 22.0f, CARD_H = 92.0f, COL = 480.0f;
        const float H = (std::min)(screen.y - 60.0f, 4.0f * (CARD_H + 10.0f) + 190.0f);
        const ImVec2 o((screen.x - W) * 0.5f, (screen.y - H) * 0.5f);
        GuiCards::panel(dl, o, ImVec2(o.x + W, o.y + H));
        const std::string title = "Movimientos de " + (species ? species->name : std::string("pokémon"));
        GuiCards::text(dl, ImVec2(o.x + PAD, o.y + 14.0f), IM_COL32(255, 255, 255, 255), title.c_str(), 1.3f);
        GuiCards::text(dl, ImVec2(o.x + PAD, o.y + 50.0f), GuiStyle::MUTED, "Elige un hueco y luego un movimiento desbloqueado", 0.9f);

        // Huecos actuales.
        float y = o.y + 80.0f;
        for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) {
            const Move* move = data.move(owned.moves[i]);
            const bool selected = detail::slot == i;
            ImGui::SetCursorScreenPos(ImVec2(o.x + PAD, y));
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##slot", ImVec2(COL, CARD_H))) detail::slot = i;
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            if (move) LearnComponent::detail::moveCard(dl, data, *move, ImVec2(o.x + PAD, y), ImVec2(COL, CARD_H), hovered || selected, storage.ppOf(owned, i));
            else {
                dl->AddRectFilled(ImVec2(o.x + PAD, y), ImVec2(o.x + PAD + COL, y + CARD_H), IM_COL32(40, 40, 52, 220), 12.0f);
                GuiCards::centeredText(dl, ImVec2(o.x + PAD + COL * 0.5f, y + CARD_H * 0.5f), GuiStyle::MUTED, "Hueco vacío", 1.0f);
            }
            if (selected) dl->AddRect(ImVec2(o.x + PAD - 3.0f, y - 3.0f), ImVec2(o.x + PAD + COL + 3.0f, y + CARD_H + 3.0f), IM_COL32(255, 215, 90, 255), 14.0f, 0, 3.0f);
            y += CARD_H + 10.0f;
        }

        // Desbloqueados (a la derecha, con desplazamiento).
        const float rx = o.x + PAD + COL + 24.0f, rw = W - PAD * 2.0f - COL - 24.0f, ry = o.y + 80.0f, rh = H - 80.0f - 66.0f;
        GuiCards::text(dl, ImVec2(rx, ry - 22.0f), IM_COL32(255, 255, 255, 235), "Desbloqueados", 0.95f);
        ImGui::SetCursorScreenPos(ImVec2(rx, ry));
        ImGui::BeginChild("##unlocked", ImVec2(rw, rh), false, ImGuiWindowFlags_NoBackground);
        ImDrawList* cl = ImGui::GetWindowDrawList();
        for (size_t i = 0; i < owned.unlocked.size(); ++i) {
            const Move* move = data.move(owned.unlocked[i]);
            if (!move) continue;
            const ImVec2 a = ImGui::GetCursorScreenPos();
            ImGui::PushID(static_cast<int>(i));
            const bool clicked = ImGui::InvisibleButton("##unlockedMove", ImVec2(rw - 10.0f, 56.0f));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            bool equipped = false;
            for (const int id : owned.moves) equipped = equipped || id == move->id;
            cl->AddRectFilled(a, ImVec2(a.x + rw - 10.0f, a.y + 56.0f), GuiCards::withAlpha(GuiCards::typeColor(move->typeId), hovered ? 255 : equipped ? 120 : 190), 10.0f);
            GuiCards::text(cl, ImVec2(a.x + 10.0f, a.y + 6.0f), IM_COL32(255, 255, 255, 255), move->name.c_str(), 0.95f);
            char info[96];
            std::snprintf(info, sizeof(info), "%s · Pot. %s · PP %d%s", LearnComponent::detail::typeName(data, move->typeId).c_str(), move->power > 0 ? std::to_string(move->power).c_str() : "—", move->pp, equipped ? "   (equipado)" : "");
            GuiCards::text(cl, ImVec2(a.x + 10.0f, a.y + 30.0f), IM_COL32(255, 255, 255, 220), info, 0.75f);
            if (hovered) ImGui::SetTooltip("%s", move->description.c_str());
            if (clicked) storage.setMove(inTeam, index, detail::slot, move->id);
            ImGui::Dummy(ImVec2(0.0f, 6.0f)); // separación (no mover el cursor a pelo: ImGui aborta si el último gesto de un child es un SetCursorScreenPos)
        }
        ImGui::EndChild();

        bool open = true;
        ImGui::SetCursorScreenPos(ImVec2(o.x + PAD, o.y + H - 50.0f));
        const bool canClear = detail::slot >= 0 && owned.moves[detail::slot] >= 0 && MoveRules::known(owned.moves) > 1;
        if (GuiCards::button("##clear", "QUITAR ESTE MOVIMIENTO", ImVec2(260.0f, 36.0f), canClear, IM_COL32(150, 70, 70, 255))) storage.clearMove(inTeam, index, detail::slot);
        ImGui::SetCursorScreenPos(ImVec2(o.x + W - PAD - 140.0f, o.y + H - 50.0f));
        if (GuiCards::button("##closeMoves", "CERRAR", ImVec2(140.0f, 36.0f), true, IM_COL32(70, 70, 92, 255)) || GuiInput::backPressed()) open = false;
        ImGui::End();
        return open;
    }
}
