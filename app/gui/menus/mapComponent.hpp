#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "imgui.h"
#include "../gameState.hpp"
#include "../game/mapTerrain.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/core/gameStatus.hpp"

// Mapa grande. Dos capas superpuestas: debajo, lo que no se ha explorado (oculto) y encima el terreno de lo ya explorado.
// A la derecha, los hábitats descubiertos; al elegir uno se ve su clima, sus recursos más comunes y la probabilidad de
// aparición de sus pokémon.
namespace MapComponent {
    namespace detail {
        inline constexpr float MARGIN = 48.0f;
        inline constexpr int MAX_ROWS = 8; // filas de cada lista del detalle

        struct Entry {
            std::string name;
            float share = 0.0f; // 0..1
        };

        // Las 'limit' entradas de mayor peso, con su porcentaje sobre el total.
        inline std::vector<Entry> topShares(std::vector<Entry> entries, size_t limit, size_t& hidden) {
            float total = 0.0f;
            for (const Entry& entry : entries) total += entry.share;
            for (Entry& entry : entries) entry.share = total > 0.0f ? entry.share / total : 0.0f;
            std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.share > b.share; });
            hidden = entries.size() > limit ? entries.size() - limit : 0;
            if (hidden) entries.resize(limit);
            return entries;
        }

        inline void drawList(ImDrawList* dl, ImVec2& pos, float width, const char* title, const std::vector<Entry>& entries, size_t hidden, ImU32 barColor) {
            GuiCards::text(dl, pos, GuiStyle::MUTED, title, 0.9f);
            pos.y += ImGui::GetFontSize() * 0.9f + 6.0f;
            if (entries.empty()) {
                GuiCards::text(dl, pos, GuiStyle::MUTED, "Sin datos", 0.85f);
                pos.y += ImGui::GetFontSize() + 8.0f;
            }
            for (const Entry& entry : entries) {
                char percent[16];
                std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(std::lround(entry.share * 100.0f)));
                GuiCards::text(dl, pos, IM_COL32(255, 255, 255, 255), entry.name.c_str(), 0.9f);
                GuiCards::text(dl, ImVec2(pos.x + width - GuiCards::textWidth(percent, 0.9f), pos.y), IM_COL32(255, 255, 255, 230), percent, 0.9f);
                GuiCards::bar(dl, ImVec2(pos.x, pos.y + ImGui::GetFontSize() * 0.9f + 2.0f), width, entry.share, barColor, 4.0f);
                pos.y += ImGui::GetFontSize() * 0.9f + 14.0f;
            }
            if (hidden) {
                char more[32];
                std::snprintf(more, sizeof(more), "+%d más", static_cast<int>(hidden));
                GuiCards::text(dl, pos, GuiStyle::MUTED, more, 0.8f);
                pos.y += ImGui::GetFontSize() * 0.8f + 6.0f;
            }
            pos.y += 8.0f;
        }

        // Detalle del hábitat: climas, recursos y pokémon, todo como porcentajes dentro de ese hábitat.
        inline void drawHabitat(ImDrawList* dl, ImVec2 pos, float width, const GameData& data, const Habitat& habitat) {
            GuiCards::text(dl, pos, IM_COL32(255, 255, 255, 255), habitat.name.c_str(), 1.3f);
            pos.y += ImGui::GetFontSize() * 1.3f + 4.0f;
            GuiCards::text(dl, pos, GuiStyle::MUTED, habitat.description.c_str(), 0.85f);
            pos.y += ImGui::GetFontSize() * 0.85f + 14.0f;

            size_t hidden = 0;
            std::vector<Entry> weather;
            for (const WeatherChance& chance : habitat.weather) if (const Weather* w = data.weather(chance.weatherId)) weather.push_back({ w->name, chance.probability });
            drawList(dl, pos, width, "CLIMAS", topShares(weather, MAX_ROWS, hidden), hidden, IM_COL32(120, 190, 255, 255));

            std::vector<Entry> resources;
            for (const ResourceChance& chance : habitat.resources) {
                for (const ResourceNodeType& node : data.nodes) if (node.id == chance.nodeId) resources.push_back({ node.name, chance.weight });
            }
            drawList(dl, pos, width, "RECURSOS MÁS COMUNES", topShares(resources, 5, hidden), hidden, IM_COL32(120, 220, 130, 255));

            std::vector<Entry> pokemon;
            for (const PokemonSpecies& species : data.species) {
                for (const HabitatWeight& entry : species.spawn.habitats) if (entry.habitatId == habitat.id) pokemon.push_back({ species.name, entry.weight });
            }
            drawList(dl, pos, width, "POKÉMON", topShares(pokemon, MAX_ROWS, hidden), hidden, IM_COL32(255, 190, 90, 255));
        }

        inline int g_selected = -1; // hábitat elegido en la lista
    }

    // ESC o el botón de volver devuelven a la pausa.
    inline void render(GameState& state, const GameStatus& status) {
        using namespace detail;
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 185));
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (GuiCards::backButton(ImVec2(MARGIN, MARGIN - 8.0f))) state = GameState::PAUSED;
        GuiCards::text(dl, ImVec2(MARGIN + 140.0f, MARGIN - 6.0f), IM_COL32(255, 255, 255, 255), "MAPA", 1.6f);
        if (GuiInput::backPressed()) state = GameState::PAUSED;
        if (!status.habitatMap || !status.data || !status.exploration) return;

        const MapTerrain::Data& terrain = MapTerrain::get(status);
        const Exploration& explored = *status.exploration;
        const GameData& data = *status.data;
        const int cells = Exploration::SIZE;

        // Hábitats descubiertos: los que tienen alguna celda explorada.
        std::vector<bool> found(data.habitats.size(), false);
        for (int row = 0; row < cells; ++row) {
            for (int col = 0; col < cells; ++col) {
                const int habitat = terrain.habitat[static_cast<size_t>(row) * cells + col];
                if (habitat >= 0 && habitat < static_cast<int>(found.size()) && explored.seen(col, row)) found[habitat] = true;
            }
        }
        if (g_selected < 0 || g_selected >= static_cast<int>(found.size()) || !found[g_selected]) {
            g_selected = -1;
            const int here = terrain.habitat[static_cast<size_t>((std::clamp)(Exploration::cellOf(status.playerZ), 0, cells - 1)) * cells +
                                             (std::clamp)(Exploration::cellOf(status.playerX), 0, cells - 1)];
            if (here >= 0 && here < static_cast<int>(found.size()) && found[here]) g_selected = here;
            for (size_t i = 0; g_selected < 0 && i < found.size(); ++i) if (found[i]) g_selected = static_cast<int>(i);
        }

        // Capa inferior (sin explorar) y capa superior (terreno explorado).
        const float top = MARGIN + 54.0f;
        const float side = (std::min)(screen.y - top - MARGIN, screen.x * 0.56f);
        const ImVec2 a(MARGIN, top), b(a.x + side, a.y + side);
        const float cell = side / static_cast<float>(cells);
        dl->AddRectFilled(ImVec2(a.x - 6.0f, a.y - 6.0f), ImVec2(b.x + 6.0f, b.y + 6.0f), IM_COL32(14, 16, 26, 245), 10.0f);
        dl->AddRectFilled(a, b, IM_COL32(34, 38, 54, 255));
        for (int row = 0; row < cells; ++row) {
            for (int col = 0; col < cells; ++col) {
                const bool seen = explored.seen(col, row);
                if (!seen) {
                    if (((row + col) & 1) == 0) continue; // tablero tenue: la zona oculta no es un bloque liso
                    dl->AddRectFilled(ImVec2(a.x + col * cell, b.y - (row + 1) * cell), ImVec2(a.x + (col + 1) * cell + 0.5f, b.y - row * cell + 0.5f), IM_COL32(40, 45, 64, 255));
                    continue;
                }
                const size_t index = static_cast<size_t>(row) * cells + col;
                ImU32 color = terrain.colors[index];
                if (g_selected >= 0 && terrain.habitat[index] != g_selected) color = IM_COL32(((color >> IM_COL32_R_SHIFT) & 0xFF) * 6 / 10, ((color >> IM_COL32_G_SHIFT) & 0xFF) * 6 / 10, ((color >> IM_COL32_B_SHIFT) & 0xFF) * 6 / 10, 255); // los demás hábitats, atenuados
                dl->AddRectFilled(ImVec2(a.x + col * cell, b.y - (row + 1) * cell), ImVec2(a.x + (col + 1) * cell + 0.5f, b.y - row * cell + 0.5f), color);
            }
        }
        dl->AddRect(a, b, IM_COL32(255, 255, 255, 120), 0.0f, 0, 1.5f);

        // Construcciones exploradas y jugador.
        const auto toScreen = [&](float x, float z) {
            return ImVec2(a.x + (x + Physics::World::HALF_SIZE) / (2.0f * Physics::World::HALF_SIZE) * side, b.y - (z + Physics::World::HALF_SIZE) / (2.0f * Physics::World::HALF_SIZE) * side);
        };
        for (const DirectX::XMFLOAT4& block : status.mapBlocks) {
            if (!explored.seen(Exploration::cellOf(block.x), Exploration::cellOf(block.y))) continue;
            const ImVec2 lo = toScreen(block.x - block.z, block.y + block.w), hi = toScreen(block.x + block.z, block.y - block.w);
            dl->AddRectFilled(lo, hi, IM_COL32(60, 52, 48, 255));
            dl->AddRect(lo, hi, IM_COL32(0, 0, 0, 220), 0.0f, 0, 1.0f);
        }
        const ImVec2 me = toScreen(status.playerX, status.playerZ);
        const float dirX = std::sin(status.playerYaw), dirY = -std::cos(status.playerYaw);
        const ImVec2 tip(me.x + dirX * 11.0f, me.y + dirY * 11.0f);
        const ImVec2 left(me.x - dirY * 7.0f - dirX * 6.0f, me.y + dirX * 7.0f - dirY * 6.0f);
        const ImVec2 right(me.x + dirY * 7.0f - dirX * 6.0f, me.y - dirX * 7.0f - dirY * 6.0f);
        dl->AddTriangleFilled(tip, left, right, IM_COL32(255, 255, 255, 255));
        dl->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 230), 2.0f);

        // Panel de información.
        const ImVec2 pa(b.x + 36.0f, a.y - 6.0f), pb(screen.x - MARGIN, b.y + 6.0f);
        GuiCards::panel(dl, pa, pb);
        GuiCards::text(dl, ImVec2(pa.x + 16.0f, pa.y + 14.0f), GuiStyle::MUTED, "HÁBITATS EXPLORADOS", 0.9f);
        float x = pa.x + 16.0f;
        const float chipY = pa.y + 14.0f + ImGui::GetFontSize() + 10.0f;
        float y = chipY;
        for (size_t i = 0; i < found.size(); ++i) {
            if (!found[i]) continue;
            const float w = GuiCards::textWidth(data.habitats[i].name.c_str(), 0.95f) + 34.0f;
            if (x + w > pb.x - 16.0f) { x = pa.x + 16.0f; y += 36.0f; }
            ImGui::SetCursorScreenPos(ImVec2(x, y));
            ImGui::PushID(static_cast<int>(i));
            const bool clicked = ImGui::InvisibleButton("##habitat", ImVec2(w, 30.0f));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            if (clicked) g_selected = static_cast<int>(i);
            const bool selected = static_cast<int>(i) == g_selected;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + 30.0f), selected ? IM_COL32(70, 90, 160, 235) : hovered ? IM_COL32(60, 64, 90, 235) : IM_COL32(44, 46, 66, 225), 15.0f);
            const DirectX::XMFLOAT3 color = HabitatStyle::color(data.habitats[i].name);
            dl->AddCircleFilled(ImVec2(x + 15.0f, y + 15.0f), 7.0f, IM_COL32(int(color.x * 255), int(color.y * 255), int(color.z * 255), 255), 16);
            dl->AddCircle(ImVec2(x + 15.0f, y + 15.0f), 7.0f, IM_COL32(255, 255, 255, 200), 16, 1.5f);
            GuiCards::text(dl, ImVec2(x + 28.0f, y + 6.0f), IM_COL32(255, 255, 255, 255), data.habitats[i].name.c_str(), 0.95f);
            x += w + 8.0f;
        }
        if (g_selected >= 0) {
            const float detailTop = y + 46.0f;
            dl->AddLine(ImVec2(pa.x + 16.0f, detailTop - 8.0f), ImVec2(pb.x - 16.0f, detailTop - 8.0f), GuiCards::EDGE, 1.0f);
            dl->PushClipRect(ImVec2(pa.x, detailTop - 6.0f), pb, true);
            drawHabitat(dl, ImVec2(pa.x + 16.0f, detailTop), pb.x - pa.x - 32.0f, data, data.habitats[g_selected]);
            dl->PopClipRect();
        }
    }
}
