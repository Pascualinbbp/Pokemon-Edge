#pragma once
#include <cctype>
#include <cstdio>
#include <string>
#include "imgui.h"
#include "../style/guiCards.hpp"
#include "../../models/gameData.hpp"

// Tarjeta de un movimiento (nombre, tipo, clase, potencia, precisión, PP y descripción) que usa la edición de movimientos.
namespace MoveCard {
    inline std::string typeName(const GameData& data, int typeId) {
        const PokemonType* type = data.type(typeId);
        if (!type) return "Normal";
        std::string name = type->name;
        if (!name.empty() && static_cast<unsigned char>(name[0]) < 128) name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        return name;
    }
    inline void card(ImDrawList* dl, const GameData& data, const Move& move, const ImVec2& a, const ImVec2& size, bool hovered, int pp = -1) {
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
