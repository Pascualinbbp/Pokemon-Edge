#pragma once
#include <algorithm>
#include <cmath>
#include "imgui.h"
#include "guiStyle.hpp"
#include "../../engine/world/pokemonStyle.hpp"
#include "../../models/element.hpp"

// Piezas visuales de los paneles del juego (menú lateral, mochila, pokémon): tarjetas redondeadas, fichas,
// casillas seleccionables, barras y gráfico hexagonal. Es el único sitio donde se define su aspecto.
namespace GuiCards {
    inline constexpr float ROUNDING = 12.0f;
    inline constexpr ImU32 GLASS  = IM_COL32(24, 24, 34, 215);  // fondo de los paneles
    inline constexpr ImU32 EDGE   = IM_COL32(255, 255, 255, 40);
    inline constexpr ImU32 SELECT = IM_COL32(255, 255, 255, 235);
    inline constexpr ImU32 SHADE  = IM_COL32(0, 0, 0, 90);

    inline ImU32 withAlpha(ImU32 color, int alpha) { return (color & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << 24); }

    inline void panel(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 fill = GLASS) {
        dl->AddRectFilled(a, b, fill, ROUNDING);
        dl->AddRect(a, b, EDGE, ROUNDING, 0, 1.5f);
    }

    inline void text(ImDrawList* dl, const ImVec2& pos, ImU32 color, const char* value, float scale = 1.0f) {
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale, pos, color, value);
    }

    inline float textWidth(const char* value, float scale = 1.0f) { return ImGui::CalcTextSize(value).x * scale; }

    inline void centeredText(ImDrawList* dl, const ImVec2& center, ImU32 color, const char* value, float scale = 1.0f) {
        text(dl, ImVec2(center.x - textWidth(value, scale) * 0.5f, center.y - ImGui::GetFontSize() * scale * 0.5f), color, value, scale);
    }

    // Ficha redondeada con texto. Devuelve su ancho.
    inline float chip(ImDrawList* dl, const ImVec2& pos, const char* value, ImU32 fill, float scale = 0.9f) {
        const float width = textWidth(value, scale) + 16.0f;
        const float height = ImGui::GetFontSize() * scale + 6.0f;
        dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), fill, height * 0.5f);
        text(dl, ImVec2(pos.x + 8.0f, pos.y + 3.0f), IM_COL32(255, 255, 255, 255), value, scale);
        return width;
    }

    inline ImU32 elementColor(int elementId) {
        int r, g, b;
        PokemonStyle::elementRgb(elementId, r, g, b);
        return IM_COL32(r, g, b, 255);
    }

    inline float elementChip(ImDrawList* dl, const ImVec2& pos, const Element& element) {
        return chip(dl, pos, element.name.c_str(), elementColor(element.id));
    }

    // Casilla cuadrada seleccionable: icono arriba, nombre abajo y una etiqueta opcional (cantidad...) en la esquina.
    // 'fill' es el color de la casilla; 'dim' la apaga (objeto que no se tiene).
    template <typename Icon>
    inline bool tile(const char* id, const ImVec2& size, bool selected, ImU32 fill, Icon drawIcon, const char* label,
                     const char* badge = "", bool dim = false) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 a = ImGui::GetCursorScreenPos();
        const ImVec2 b(a.x + size.x, a.y + size.y);
        const bool clicked = ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();

        const int boost = hovered ? 30 : 0;
        const auto lift = [&](ImU32 c) {
            const int r = (std::min)(255, static_cast<int>(c & 0xFF) + boost);
            const int g = (std::min)(255, static_cast<int>((c >> 8) & 0xFF) + boost);
            const int bl = (std::min)(255, static_cast<int>((c >> 16) & 0xFF) + boost);
            return IM_COL32(r, g, bl, dim ? 120 : 255);
        };
        dl->AddRectFilled(a, b, lift(fill), 10.0f);
        dl->AddRectFilled(ImVec2(a.x, a.y + size.y * 0.62f), b, SHADE, 10.0f, ImDrawFlags_RoundCornersBottom);
        drawIcon(dl, ImVec2(a.x + size.x * 0.5f, a.y + size.y * 0.38f), size.x * 0.27f);

        dl->PushClipRect(a, b, true);
        centeredText(dl, ImVec2(a.x + size.x * 0.5f, b.y - 12.0f), IM_COL32(255, 255, 255, dim ? 150 : 255), label, 0.85f);
        if (badge[0]) {
            const float w = textWidth(badge, 0.85f) + 10.0f;
            dl->AddRectFilled(ImVec2(b.x - w - 4.0f, a.y + 4.0f), ImVec2(b.x - 4.0f, a.y + 24.0f), IM_COL32(0, 0, 0, 150), 8.0f);
            text(dl, ImVec2(b.x - w + 1.0f, a.y + 7.0f), IM_COL32(255, 255, 255, 255), badge, 0.85f);
        }
        dl->PopClipRect();
        if (selected) dl->AddRect(a, b, SELECT, 10.0f, 0, 3.0f);
        return clicked;
    }

    // Barra horizontal 0..1.
    inline void bar(ImDrawList* dl, const ImVec2& pos, float width, float value, ImU32 color, float height = 8.0f) {
        dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(255, 255, 255, 30), height * 0.5f);
        const float filled = width * (std::clamp)(value, 0.0f, 1.0f);
        if (filled > 0.5f) dl->AddRectFilled(pos, ImVec2(pos.x + filled, pos.y + height), color, height * 0.5f);
    }

    // Gráfico hexagonal de 6 valores (0..1, empezando arriba y en sentido horario).
    inline void radar(ImDrawList* dl, const ImVec2& center, float radius, const float (&values)[6], ImU32 color) {
        constexpr float PI = 3.14159265f;
        ImVec2 ring[3][6];
        ImVec2 shape[6];
        for (int i = 0; i < 6; ++i) {
            const float angle = -PI * 0.5f + static_cast<float>(i) * PI / 3.0f;
            const float cx = std::cos(angle), cy = std::sin(angle);
            for (int r = 0; r < 3; ++r) {
                const float rr = radius * static_cast<float>(r + 1) / 3.0f;
                ring[r][i] = ImVec2(center.x + cx * rr, center.y + cy * rr);
            }
            const float v = radius * (std::clamp)(values[i], 0.05f, 1.0f);
            shape[i] = ImVec2(center.x + cx * v, center.y + cy * v);
        }
        for (int r = 0; r < 3; ++r) {
            for (int i = 0; i < 6; ++i) dl->AddLine(ring[r][i], ring[r][(i + 1) % 6], IM_COL32(255, 255, 255, 50), 1.0f);
        }
        for (int i = 0; i < 6; ++i) dl->AddLine(center, ring[2][i], IM_COL32(255, 255, 255, 35), 1.0f);
        for (int i = 0; i < 6; ++i) dl->AddTriangleFilled(center, shape[i], shape[(i + 1) % 6], withAlpha(color, 90));
        for (int i = 0; i < 6; ++i) dl->AddLine(shape[i], shape[(i + 1) % 6], color, 2.0f);
    }

    // Botón rectangular redondeado con texto centrado. Devuelve true al pulsarlo; 'enabled' false lo apaga.
    inline bool button(const char* id, const char* label, const ImVec2& size, bool enabled = true, ImU32 fill = GuiStyle::ACCENT) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 a = ImGui::GetCursorScreenPos();
        const ImVec2 b(a.x + size.x, a.y + size.y);
        const bool clicked = ImGui::InvisibleButton(id, size) && enabled;
        const bool hovered = ImGui::IsItemHovered() && enabled;
        dl->AddRectFilled(a, b, enabled ? (hovered ? withAlpha(fill, 255) : withAlpha(fill, 215)) : IM_COL32(70, 70, 80, 200), size.y * 0.5f);
        centeredText(dl, ImVec2(a.x + size.x * 0.5f, a.y + size.y * 0.5f), IM_COL32(255, 255, 255, enabled ? 255 : 130), label, 0.95f);
        return clicked;
    }
}
