#pragma once
#include <cstdio>
#include <string>
#include "imgui.h"
#include "learnComponent.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/world/state/pokemonStorage.hpp"

// Edición de los movimientos de un pokémon: a la izquierda los 4 que lleva y a la derecha los que tiene desbloqueados (los que intentó aprender).
// Todo se hace arrastrando: un movimiento sobre otro de los 4 los intercambia (reordenar) y uno desbloqueado sobre uno de los 4 lo sustituye.
// Va como pestaña de la ficha del pokémon (content) o como ventana propia a pantalla completa (render, desde el PC). Devuelve false cuando se cierra.
namespace MoveEditor {
    namespace detail {
        inline int slot = 0;                 // hueco elegido con un clic (para quitarlo)
        struct Drag { int kind = 0; int slot = -1; int moveId = -1; }; // kind 1 = uno de los 4, 2 = uno desbloqueado
        inline Drag drag;
    }

    inline void reset() { detail::slot = 0; detail::drag = {}; }

    // Contenido dentro de un rectángulo ('o', ancho W, alto H); 'embedded' = sin marco, título ni botón de cerrar.
    inline void content(ImDrawList* dl, const GameData& data, PokemonStorage& storage, bool inTeam, int index, const OwnedPokemon& owned, const PokemonSpecies* species, const ImVec2& o, float W, float H, bool embedded) {
        constexpr float PAD = 22.0f, CARD_H = 92.0f;
        const float COL = (std::min)(480.0f, (W - PAD * 3.0f) * 0.55f);
        if (!embedded) {
            GuiCards::panel(dl, o, ImVec2(o.x + W, o.y + H));
            const std::string title = "Movimientos de " + (species ? species->name : std::string("pokémon"));
            GuiCards::text(dl, ImVec2(o.x + PAD, o.y + 14.0f), IM_COL32(255, 255, 255, 255), title.c_str(), 1.3f);
        }
        const float top = embedded ? 34.0f : 80.0f;
        GuiCards::text(dl, ImVec2(o.x + PAD, o.y + (embedded ? 8.0f : 50.0f)), GuiStyle::MUTED, "Arrastra un movimiento sobre otro para intercambiarlos; uno desbloqueado sobre uno de los 4 lo sustituye", 0.85f);

        ImVec2 slotA[MoveRules::MAX_KNOWN], slotB[MoveRules::MAX_KNOWN];
        float y = o.y + top;
        for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) {
            const Move* move = data.move(owned.moves[i]);
            const bool selected = detail::slot == i;
            slotA[i] = ImVec2(o.x + PAD, y);
            slotB[i] = ImVec2(o.x + PAD + COL, y + CARD_H);
            ImGui::SetCursorScreenPos(slotA[i]);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##slot", ImVec2(COL, CARD_H))) detail::slot = i;
            const bool hovered = ImGui::IsItemHovered();
            if (move && detail::drag.kind == 0 && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) detail::drag = { 1, i, move->id };
            ImGui::PopID();
            const bool target = detail::drag.kind != 0 && hovered && !(detail::drag.kind == 1 && detail::drag.slot == i);
            const bool lifted = detail::drag.kind == 1 && detail::drag.slot == i;
            if (move) LearnComponent::detail::moveCard(dl, data, *move, slotA[i], ImVec2(COL, CARD_H), (hovered && detail::drag.kind == 0) || selected || target, storage.ppOf(owned, i));
            else {
                dl->AddRectFilled(slotA[i], slotB[i], IM_COL32(40, 40, 52, 220), 12.0f);
                GuiCards::centeredText(dl, ImVec2(o.x + PAD + COL * 0.5f, y + CARD_H * 0.5f), GuiStyle::MUTED, "Hueco vacío", 1.0f);
            }
            if (lifted) dl->AddRectFilled(slotA[i], slotB[i], IM_COL32(0, 0, 0, 120), 12.0f);
            if (selected) dl->AddRect(ImVec2(slotA[i].x - 3.0f, y - 3.0f), ImVec2(slotB[i].x + 3.0f, y + CARD_H + 3.0f), IM_COL32(255, 215, 90, 255), 14.0f, 0, 3.0f);
            if (target) dl->AddRect(ImVec2(slotA[i].x - 3.0f, y - 3.0f), ImVec2(slotB[i].x + 3.0f, y + CARD_H + 3.0f), GuiCards::SELECT, 14.0f, 0, 3.0f);
            y += CARD_H + 10.0f;
        }

        // Desbloqueados (a la derecha, con desplazamiento).
        const float rx = o.x + PAD + COL + 24.0f, rw = W - PAD * 2.0f - COL - 24.0f, ry = o.y + top + (embedded ? 4.0f : 0.0f), rh = H - top - (embedded ? 70.0f : 66.0f);
        GuiCards::text(dl, ImVec2(rx, ry - 22.0f), IM_COL32(255, 255, 255, 235), "Desbloqueados", 0.95f);
        ImGui::SetCursorScreenPos(ImVec2(rx, ry));
        ImGui::BeginChild(embedded ? "##unlockedTab" : "##unlocked", ImVec2(rw, rh), false, ImGuiWindowFlags_NoBackground);
        ImDrawList* cl = ImGui::GetWindowDrawList();
        for (size_t i = 0; i < owned.unlocked.size(); ++i) {
            const Move* move = data.move(owned.unlocked[i]);
            if (!move) continue;
            const ImVec2 a = ImGui::GetCursorScreenPos();
            ImGui::PushID(static_cast<int>(i));
            ImGui::InvisibleButton("##unlockedMove", ImVec2(rw - 10.0f, 56.0f));
            const bool hovered = ImGui::IsItemHovered();
            if (detail::drag.kind == 0 && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) detail::drag = { 2, -1, move->id };
            ImGui::PopID();
            bool equipped = false;
            for (const int id : owned.moves) equipped = equipped || id == move->id;
            cl->AddRectFilled(a, ImVec2(a.x + rw - 10.0f, a.y + 56.0f), GuiCards::withAlpha(GuiCards::typeColor(move->typeId), hovered ? 255 : equipped ? 120 : 190), 10.0f);
            GuiCards::text(cl, ImVec2(a.x + 10.0f, a.y + 6.0f), IM_COL32(255, 255, 255, 255), move->name.c_str(), 0.95f);
            char info[96];
            std::snprintf(info, sizeof(info), "%s · Pot. %s · PP %d%s", LearnComponent::detail::typeName(data, move->typeId).c_str(), move->power > 0 ? std::to_string(move->power).c_str() : "—", move->pp, equipped ? "   (equipado)" : "");
            GuiCards::text(cl, ImVec2(a.x + 10.0f, a.y + 30.0f), IM_COL32(255, 255, 255, 220), info, 0.75f);
            if (hovered && detail::drag.kind == 0) ImGui::SetTooltip("%s", move->description.c_str());
            ImGui::Dummy(ImVec2(0.0f, 6.0f)); // separación (no mover el cursor a pelo: ImGui aborta si el último gesto de un child es un SetCursorScreenPos)
        }
        ImGui::EndChild();

        // El movimiento que se arrastra, pegado al cursor; al soltar sobre uno de los 4 se intercambia o se sustituye.
        if (detail::drag.kind != 0) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            if (const Move* dragged = data.move(detail::drag.moveId)) {
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                const float w = 230.0f;
                fg->AddRectFilled(ImVec2(mouse.x - w * 0.5f, mouse.y - 22.0f), ImVec2(mouse.x + w * 0.5f, mouse.y + 22.0f), GuiCards::withAlpha(GuiCards::typeColor(dragged->typeId), 235), 12.0f);
                fg->AddRect(ImVec2(mouse.x - w * 0.5f, mouse.y - 22.0f), ImVec2(mouse.x + w * 0.5f, mouse.y + 22.0f), IM_COL32(255, 255, 255, 220), 12.0f, 0, 2.0f);
                GuiCards::centeredText(fg, mouse, IM_COL32(255, 255, 255, 255), dragged->name.c_str(), 1.0f);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) {
                    if (mouse.x < slotA[i].x || mouse.x > slotB[i].x || mouse.y < slotA[i].y || mouse.y > slotB[i].y) continue;
                    if (detail::drag.kind == 1) storage.swapMoves(inTeam, index, detail::drag.slot, i);
                    else storage.setMove(inTeam, index, i, detail::drag.moveId);
                    detail::slot = i;
                    break;
                }
                detail::drag = {};
            }
        }

        const float by = o.y + H - (embedded ? 44.0f : 50.0f);
        ImGui::SetCursorScreenPos(ImVec2(o.x + PAD, by));
        const bool canClear = detail::slot >= 0 && owned.moves[detail::slot] >= 0 && MoveRules::known(owned.moves) > 1;
        if (GuiCards::button("##clear", "QUITAR ESTE MOVIMIENTO", ImVec2(260.0f, 36.0f), canClear, IM_COL32(150, 70, 70, 255))) storage.clearMove(inTeam, index, detail::slot);
    }

    inline bool render(const GameData& data, PokemonStorage& storage, bool inTeam, int index) {
        const std::vector<OwnedPokemon>& list = inTeam ? storage.team() : storage.pc();
        if (index < 0 || index >= static_cast<int>(list.size())) return false;
        const OwnedPokemon owned = list[static_cast<size_t>(index)];
        const PokemonSpecies* species = data.speciesById(owned.speciesId);
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        // El editor es su propia ventana a pantalla completa y siempre la de más arriba: nada del resto de la interfaz se dibuja encima ni recibe clics.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(screen);
        ImGui::SetNextWindowFocus();
        ImGui::Begin("##moveEditor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), screen, IM_COL32(0, 0, 0, 170));
        constexpr float W = 1040.0f;
        const float H = (std::min)(screen.y - 60.0f, 4.0f * (92.0f + 10.0f) + 190.0f);
        const ImVec2 o((screen.x - W) * 0.5f, (screen.y - H) * 0.5f);
        content(dl, data, storage, inTeam, index, owned, species, o, W, H, false);
        bool open = true;
        ImGui::SetCursorScreenPos(ImVec2(o.x + W - 22.0f - 140.0f, o.y + H - 50.0f));
        if (GuiCards::button("##closeMoves", "CERRAR", ImVec2(140.0f, 36.0f), true, IM_COL32(70, 70, 92, 255)) || GuiInput::backPressed()) { open = false; detail::drag = {}; }
        ImGui::End();
        return open;
    }
}
