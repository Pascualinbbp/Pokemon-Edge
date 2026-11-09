#pragma once
#include <cstdio>
#include <string>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/core/gameEngine.hpp"

// Un pokémon quiere aprender un movimiento y ya conoce cuatro: "¿Quieres que olvide uno?". Si se acepta se eligen los que olvida (con opción de cancelar y
// dejarlo sin aprender); aun rechazado, el movimiento queda desbloqueado y se puede poner desde la edición de movimientos del menú de pokémon.
namespace LearnComponent {
    namespace detail {
        inline bool picking = false;
        inline std::string typeName(const GameData& data, int typeId) {
            const PokemonType* type = data.type(typeId);
            if (!type) return "Normal";
            std::string name = type->name;
            if (!name.empty() && static_cast<unsigned char>(name[0]) < 128) name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            return name;
        }
        inline void moveCard(ImDrawList* dl, const GameData& data, const Move& move, const ImVec2& a, const ImVec2& size, bool hovered, int pp = -1) {
            dl->AddRectFilled(a, ImVec2(a.x + size.x, a.y + size.y), GuiCards::withAlpha(GuiCards::typeColor(move.typeId), hovered ? 255 : 205), 12.0f);
            if (hovered) dl->AddRect(ImVec2(a.x - 2.0f, a.y - 2.0f), ImVec2(a.x + size.x + 2.0f, a.y + size.y + 2.0f), GuiCards::SELECT, 14.0f, 0, 2.0f);
            GuiCards::text(dl, ImVec2(a.x + 12.0f, a.y + 8.0f), IM_COL32(255, 255, 255, 255), move.name.c_str(), 1.05f);
            char info[96];
            std::snprintf(info, sizeof(info), "%s · %s · Pot. %s · Prec. %s", typeName(data, move.typeId).c_str(), move.kind == MoveClass::Kind::PHYSICAL ? "Físico" : move.kind == MoveClass::Kind::SPECIAL ? "Especial" : "Estado",
                          move.power > 0 ? std::to_string(move.power).c_str() : "—", move.accuracy > 0 ? std::to_string(move.accuracy).c_str() : "—");
            GuiCards::text(dl, ImVec2(a.x + 12.0f, a.y + 34.0f), IM_COL32(255, 255, 255, 225), info, 0.75f);
            if (pp >= 0) { std::snprintf(info, sizeof(info), "PP %d", pp); GuiCards::text(dl, ImVec2(a.x + size.x - 12.0f - GuiCards::textWidth(info, 0.85f), a.y + 8.0f), IM_COL32(255, 255, 255, 235), info, 0.85f); }
            ImGui::PushTextWrapPos(a.x + size.x - 10.0f - ImGui::GetWindowPos().x);
            ImGui::SetCursorScreenPos(ImVec2(a.x + 12.0f, a.y + 56.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 215));
            ImGui::TextUnformatted(move.description.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
        }
    }

    inline void reset() { detail::picking = false; }

    inline void render(GameState& state, GameState back, GameEngine& engine) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 160));
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const GameData& data = engine.data();
        PokemonStorage& storage = engine.storage();
        const PokemonStorage::LearnRequest request = storage.pendingLearnRequest();
        const Move* learning = data.move(request.moveId);
        const OwnedPokemon* owned = request.index >= 0 ? (request.inTeam ? &storage.team()[static_cast<size_t>(request.index)] : nullptr) : nullptr;
        if (!owned && request.index >= 0 && !request.inTeam) owned = &storage.pc()[static_cast<size_t>(request.index)];
        const PokemonSpecies* species = owned ? data.speciesById(owned->speciesId) : nullptr;
        if (!learning || !owned || !species) { detail::picking = false; state = back; return; }

        constexpr float W = 720.0f, CARD_H = 92.0f, PAD = 24.0f;
        const float height = detail::picking ? 140.0f + 4.0f * (CARD_H + 10.0f) + 70.0f : 330.0f;
        const ImVec2 origin((screen.x - W) * 0.5f, (screen.y - height) * 0.5f);
        GuiCards::panel(dl, origin, ImVec2(origin.x + W, origin.y + height));
        ItemIcon::creature(dl, ImVec2(origin.x + PAD + 26.0f, origin.y + PAD + 28.0f), 24.0f, species->id);
        const std::string title = species->name + " quiere aprender " + learning->name;
        GuiCards::text(dl, ImVec2(origin.x + PAD + 66.0f, origin.y + PAD), IM_COL32(255, 255, 255, 255), title.c_str(), 1.25f);
        GuiCards::text(dl, ImVec2(origin.x + PAD + 66.0f, origin.y + PAD + 30.0f), GuiStyle::MUTED, "pero ya conoce 4 movimientos.", 0.95f);
        detail::moveCard(dl, data, *learning, ImVec2(origin.x + PAD, origin.y + PAD + 62.0f), ImVec2(W - PAD * 2.0f, CARD_H), false);

        if (!detail::picking) {
            GuiCards::text(dl, ImVec2(origin.x + PAD, origin.y + PAD + 62.0f + CARD_H + 16.0f), IM_COL32(255, 255, 255, 255), "¿Quieres que olvide uno para aprenderlo?", 1.0f);
            const float bw = (W - PAD * 2.0f - 14.0f) * 0.5f, by = origin.y + height - PAD - 46.0f;
            ImGui::SetCursorScreenPos(ImVec2(origin.x + PAD, by));
            if (GuiCards::button("##yes", "SÍ, OLVIDAR UNO", ImVec2(bw, 46.0f), true, IM_COL32(60, 150, 90, 255))) detail::picking = true;
            ImGui::SetCursorScreenPos(ImVec2(origin.x + PAD + bw + 14.0f, by));
            if (GuiCards::button("##no", "NO APRENDERLO", ImVec2(bw, 46.0f), true, IM_COL32(150, 70, 70, 255)) || GuiInput::backPressed()) {
                storage.resolveLearn(request.inTeam, request.index, -1);
                state = back;
            }
            GuiCards::text(dl, ImVec2(origin.x + PAD, origin.y + height + 10.0f), GuiStyle::MUTED, "Si no lo aprende, quedará desbloqueado: podrás equiparlo luego desde Pokémon > Movimientos.", 0.85f);
        } else {
            GuiCards::text(dl, ImVec2(origin.x + PAD, origin.y + PAD + 62.0f + CARD_H + 10.0f), IM_COL32(255, 255, 255, 255), "¿Cuál olvida? (clic en el movimiento que se sustituye)", 1.0f);
            float y = origin.y + PAD + 62.0f + CARD_H + 40.0f;
            for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) {
                const Move* known = data.move(owned->moves[i]);
                ImGui::SetCursorScreenPos(ImVec2(origin.x + PAD, y));
                ImGui::PushID(i);
                const bool clicked = ImGui::InvisibleButton("##slot", ImVec2(W - PAD * 2.0f, CARD_H));
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                if (known) detail::moveCard(dl, data, *known, ImVec2(origin.x + PAD, y), ImVec2(W - PAD * 2.0f, CARD_H), hovered, storage.ppOf(*owned, i));
                if (clicked) { storage.resolveLearn(request.inTeam, request.index, i); detail::picking = false; if (storage.pendingLearnRequest().index < 0) state = back; return; }
                y += CARD_H + 10.0f;
            }
            ImGui::SetCursorScreenPos(ImVec2(origin.x + PAD, y + 6.0f));
            if (GuiCards::button("##cancel", "CANCELAR (no aprenderlo)", ImVec2(260.0f, 38.0f), true, IM_COL32(70, 70, 92, 255)) || GuiInput::backPressed()) detail::picking = false;
        }
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
}
