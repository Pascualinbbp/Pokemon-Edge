#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../models/gameData.hpp"

// Tabla de tipos: una fila por tipo atacante y una columna por tipo defensor; cada casilla se colorea según lo que multiplica el daño
// (x2 verde, x1/2 naranja, x0 gris oscuro, x1 vacía). Al pasar el ratón se resalta la fila y la columna y se explica la casilla.
namespace TypeChartComponent {
    namespace detail {
        inline constexpr float MARGIN = 48.0f;
        inline constexpr ImU32 SUPER = IM_COL32(70, 180, 95, 255);
        inline constexpr ImU32 WEAK = IM_COL32(225, 140, 50, 255);
        inline constexpr ImU32 IMMUNE = IM_COL32(35, 35, 45, 255);
        inline constexpr ImU32 NORMAL = IM_COL32(255, 255, 255, 14);

        // Primeras letras del nombre (en UTF-8: por caracteres, no por bytes), con la inicial en mayúscula.
        inline std::string shortName(const std::string& name, int letters) {
            std::string out;
            int count = 0;
            for (size_t i = 0; i < name.size() && count < letters;) {
                size_t length = 1;
                while (i + length < name.size() && (static_cast<unsigned char>(name[i + length]) & 0xC0) == 0x80) ++length;
                out += name.substr(i, length);
                i += length;
                ++count;
            }
            if (!out.empty() && static_cast<unsigned char>(out[0]) < 128) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
            return out;
        }

        inline std::string title(const std::string& name) { return shortName(name, 64); }

        inline ImU32 cellColor(float multiplier) {
            if (multiplier <= 0.0f) return IMMUNE;
            if (multiplier > 1.0f) return SUPER;
            if (multiplier < 1.0f) return WEAK;
            return NORMAL;
        }

        inline const char* cellLabel(float multiplier) { return multiplier <= 0.0f ? "0" : multiplier > 1.0f ? "2" : multiplier < 1.0f ? "1/2" : ""; }

        // Orden de la tabla: columnas por id con Normal al final; filas: por cada columna, el atacante que no le hace nada (x0), y después los
        // que sobran en orden de columna (Normal al final).
        struct Order { std::vector<int> columns, rows; };
        inline Order order(const GameData& data) {
            const int count = static_cast<int>(data.types.size());
            std::vector<int> columns(count);
            for (int i = 0; i < count; ++i) columns[i] = i;
            const auto isNormal = [&](int i) { return shortName(data.types[i].name, 64) == "Normal"; };
            std::stable_sort(columns.begin(), columns.end(), [&](int a, int b) {
                if (isNormal(a) != isNormal(b)) return isNormal(b);
                return data.types[a].id < data.types[b].id;
            });
            std::vector<int> rows;
            std::vector<bool> used(count, false);
            const auto take = [&](int i) { used[i] = true; rows.push_back(i); };
            for (const int column : columns) {
                for (const int attacker : columns) {
                    if (used[attacker] || isNormal(attacker)) continue;
                    if (data.typeChart.multiplier(data.types[attacker].id, data.types[column].id) <= 0.0f) { take(attacker); break; }
                }
            }
            for (const int attacker : columns) if (!used[attacker]) take(attacker);
            return { columns, rows };
        }

        inline void legend(ImDrawList* dl, ImVec2 at) {
            struct Entry { ImU32 color; const char* text; };
            const Entry entries[] = { { SUPER, "x2  muy eficaz" }, { WEAK, "x1/2  poco eficaz" }, { IMMUNE, "x0  sin efecto" }, { NORMAL, "x1  normal" } };
            for (const Entry& entry : entries) {
                dl->AddRectFilled(at, ImVec2(at.x + 20.0f, at.y + 20.0f), entry.color, 5.0f);
                dl->AddRect(at, ImVec2(at.x + 20.0f, at.y + 20.0f), GuiCards::EDGE, 5.0f);
                GuiCards::text(dl, ImVec2(at.x + 28.0f, at.y + 1.0f), IM_COL32(255, 255, 255, 230), entry.text, 0.95f);
                at.x += 40.0f + GuiCards::textWidth(entry.text, 0.95f);
            }
        }
    }

    // ESC, el botón de volver o el mando cierran la tabla.
    inline void render(GameState& state, GameState back, const GameData& data) {
        using namespace detail;
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 170));
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (GuiCards::backButton(ImVec2(MARGIN, MARGIN - 8.0f))) state = back;
        GuiCards::text(dl, ImVec2(MARGIN + 140.0f, MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "TABLA DE TIPOS", 1.6f);
        legend(dl, ImVec2(screen.x - MARGIN - 640.0f, MARGIN));
        GuiCards::text(dl, ImVec2(MARGIN, screen.y - MARGIN), GuiStyle::MUTED, "Filas: tipo que ataca   Columnas: tipo que defiende   ESC: volver", 0.9f);

        const int count = static_cast<int>(data.types.size());
        if (count == 0) {
            if (GuiInput::backPressed()) state = back;
            return;
        }
        const Order sequence = order(data);
        const float top = MARGIN + 52.0f;
        const float available = (std::min)(screen.x - MARGIN * 2.0f, screen.y - top - MARGIN - 28.0f);
        const float cell = (std::min)(46.0f, available / (static_cast<float>(count) + 1.6f));
        const float header = cell * 1.6f;
        const float width = header + cell * count;
        const ImVec2 origin((screen.x - width) * 0.5f, top);
        GuiCards::panel(dl, ImVec2(origin.x - 14.0f, origin.y - 12.0f), ImVec2(origin.x + width + 14.0f, origin.y + header + cell * count + 12.0f));

        // Casilla bajo el ratón.
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const int hotColumn = mouse.x >= origin.x + header ? static_cast<int>((mouse.x - origin.x - header) / cell) : -1;
        const int hotRow = mouse.y >= origin.y + header ? static_cast<int>((mouse.y - origin.y - header) / cell) : -1;
        const bool hot = hotColumn >= 0 && hotColumn < count && hotRow >= 0 && hotRow < count;

        for (int c = 0; c < count; ++c) { // cabecera de columnas: defensores
            const ImVec2 a(origin.x + header + c * cell + 2.0f, origin.y + 2.0f);
            const ImVec2 b(a.x + cell - 4.0f, origin.y + header - 4.0f);
            dl->AddRectFilled(a, b, GuiCards::typeColor(data.types[sequence.columns[c]].id), 6.0f);
            if (hot && hotColumn == c) dl->AddRect(a, b, GuiCards::SELECT, 6.0f, 0, 2.0f);
            GuiCards::centeredText(dl, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), IM_COL32(255, 255, 255, 255), shortName(data.types[sequence.columns[c]].name, 3).c_str(), 0.8f);
        }
        for (int r = 0; r < count; ++r) { // cabecera de filas: atacantes
            const ImVec2 a(origin.x + 2.0f, origin.y + header + r * cell + 2.0f);
            const ImVec2 b(origin.x + header - 4.0f, a.y + cell - 4.0f);
            dl->AddRectFilled(a, b, GuiCards::typeColor(data.types[sequence.rows[r]].id), 6.0f);
            if (hot && hotRow == r) dl->AddRect(a, b, GuiCards::SELECT, 6.0f, 0, 2.0f);
            GuiCards::centeredText(dl, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), IM_COL32(255, 255, 255, 255), shortName(data.types[sequence.rows[r]].name, 5).c_str(), 0.8f);
            for (int c = 0; c < count; ++c) {
                const float multiplier = data.typeChart.multiplier(data.types[sequence.rows[r]].id, data.types[sequence.columns[c]].id);
                const ImVec2 ca(origin.x + header + c * cell + 2.0f, a.y);
                const ImVec2 cb(ca.x + cell - 4.0f, ca.y + cell - 4.0f);
                const bool line = hot && (hotRow == r || hotColumn == c);
                dl->AddRectFilled(ca, cb, cellColor(multiplier), 6.0f);
                if (line) dl->AddRectFilled(ca, cb, IM_COL32(255, 255, 255, 36), 6.0f);
                if (hot && hotRow == r && hotColumn == c) dl->AddRect(ca, cb, GuiCards::SELECT, 6.0f, 0, 2.5f);
                if (multiplier != 1.0f) GuiCards::centeredText(dl, ImVec2((ca.x + cb.x) * 0.5f, (ca.y + cb.y) * 0.5f), IM_COL32(255, 255, 255, 255), cellLabel(multiplier), 0.85f);
            }
        }

        if (hot) { // explicación de la casilla señalada
            const float multiplier = data.typeChart.multiplier(data.types[sequence.rows[hotRow]].id, data.types[sequence.columns[hotColumn]].id);
            char text[160];
            std::snprintf(text, sizeof(text), "%s  ataca a  %s:  %s", title(data.types[sequence.rows[hotRow]].name).c_str(), title(data.types[sequence.columns[hotColumn]].name).c_str(),
                          multiplier <= 0.0f ? "no le afecta" : multiplier > 1.0f ? "muy eficaz (x2)" : multiplier < 1.0f ? "poco eficaz (x1/2)" : "daño normal (x1)");
            GuiCards::text(dl, ImVec2(MARGIN, screen.y - MARGIN - 28.0f), IM_COL32(255, 215, 90, 255), text, 1.1f);
        }
        if (GuiInput::backPressed()) state = back;
    }
}
