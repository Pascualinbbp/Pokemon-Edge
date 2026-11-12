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
        inline bool dragging = false; // el movimiento nuevo se está arrastrando
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

    inline void reset() { detail::picking = false; detail::dragging = false; }

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
        if (!learning || !owned || !species) { detail::picking = false; detail::dragging = false; state = back; return; }

        constexpr float W = 720.0f, CARD_H = 92.0f, PAD = 24.0f;
        const float height = PAD + 62.0f + CARD_H + 40.0f + 4.0f * (CARD_H + 10.0f) + 64.0f;
        const ImVec2 origin((screen.x - W) * 0.5f, (screen.y - height) * 0.5f);
        GuiCards::panel(dl, origin, ImVec2(origin.x + W, origin.y + height));
        ItemIcon::creature(dl, ImVec2(origin.x + PAD + 26.0f, origin.y + PAD + 28.0f), 24.0f, species->id);
        const std::string title = species->name + " quiere aprender " + learning->name;
        GuiCards::text(dl, ImVec2(origin.x + PAD + 66.0f, origin.y + PAD), IM_COL32(255, 255, 255, 255), title.c_str(), 1.25f);
        GuiCards::text(dl, ImVec2(origin.x + PAD + 66.0f, origin.y + PAD + 30.0f), GuiStyle::MUTED, "pero ya conoce 4 movimientos. Arrastra el nuevo sobre el que olvida.", 0.95f);

        // El movimiento nuevo se arrastra sobre el que se sustituye.
        const ImVec2 newPos(origin.x + PAD, origin.y + PAD + 62.0f);
        ImGui::SetCursorScreenPos(newPos);
        ImGui::InvisibleButton("##new", ImVec2(W - PAD * 2.0f, CARD_H));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) detail::dragging = true;
        detail::moveCard(dl, data, *learning, newPos, ImVec2(W - PAD * 2.0f, CARD_H), ImGui::IsItemHovered() || detail::dragging);
        if (detail::dragging) dl->AddRectFilled(newPos, ImVec2(newPos.x + W - PAD * 2.0f, newPos.y + CARD_H), IM_COL32(0, 0, 0, 110), 12.0f);

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        int dropSlot = -1;
        float y = newPos.y + CARD_H + 40.0f;
        GuiCards::text(dl, ImVec2(origin.x + PAD, y - 28.0f), IM_COL32(255, 255, 255, 235), "Movimientos actuales", 0.95f);
        for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) {
            const Move* known = data.move(owned->moves[i]);
            const ImVec2 a(origin.x + PAD, y), b(origin.x + W - PAD, y + CARD_H);
            const bool over = mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y;
            if (known) detail::moveCard(dl, data, *known, a, ImVec2(W - PAD * 2.0f, CARD_H), detail::dragging && over, storage.ppOf(*owned, i));
            if (detail::dragging && over) { dropSlot = i; dl->AddRect(ImVec2(a.x - 3.0f, a.y - 3.0f), ImVec2(b.x + 3.0f, b.y + 3.0f), GuiCards::SELECT, 14.0f, 0, 3.0f); }
            y += CARD_H + 10.0f;
        }
        if (detail::dragging) {
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            const float w = 260.0f;
            fg->AddRectFilled(ImVec2(mouse.x - w * 0.5f, mouse.y - 22.0f), ImVec2(mouse.x + w * 0.5f, mouse.y + 22.0f), GuiCards::withAlpha(GuiCards::typeColor(learning->typeId), 240), 12.0f);
            fg->AddRect(ImVec2(mouse.x - w * 0.5f, mouse.y - 22.0f), ImVec2(mouse.x + w * 0.5f, mouse.y + 22.0f), IM_COL32(255, 255, 255, 220), 12.0f, 0, 2.0f);
            GuiCards::centeredText(fg, mouse, IM_COL32(255, 255, 255, 255), learning->name.c_str(), 1.0f);
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                detail::dragging = false;
                if (dropSlot >= 0) {
                    storage.resolveLearn(request.inTeam, request.index, dropSlot);
                    if (storage.pendingLearnRequest().index < 0) state = back;
                    return;
                }
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x + PAD, origin.y + height - PAD - 36.0f));
        if (GuiCards::button("##no", "NO APRENDERLO", ImVec2(220.0f, 36.0f), true, IM_COL32(150, 70, 70, 255)) || GuiInput::backPressed()) {
            detail::dragging = false;
            storage.resolveLearn(request.inTeam, request.index, -1);
            if (storage.pendingLearnRequest().index < 0) state = back;
        }
        GuiCards::text(dl, ImVec2(origin.x + PAD + 236.0f, origin.y + height - PAD - 28.0f), GuiStyle::MUTED, "Si no lo aprende, queda desbloqueado: Pokémon > Movimientos.", 0.8f);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
}
