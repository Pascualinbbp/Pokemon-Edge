#pragma once
#include <algorithm>
#include <initializer_list>
#include "imgui.h"
#include "guiStyle.hpp"

// Primitivas de dibujo con ImDrawList (teclas, ratón, botones de mando).
// Ojo: no usar "min"/"max" como nombres de variable, windows.h los define como macros.
namespace GuiDraw {
    inline constexpr float KEY_SIZE = 40.0f;
    inline constexpr float KEY_GAP = 4.0f;

    enum class Face { NORTH, EAST, SOUTH, WEST };

    inline ImVec2 offset(const ImVec2& origin, float x, float y) {
        return ImVec2(origin.x + x, origin.y + y);
    }

    // Texto cuya línea vertical queda centrada en y.
    inline void label(ImDrawList* dl, float x, float y, const char* text) {
        dl->AddText(ImVec2(x, y - ImGui::GetTextLineHeight() * 0.5f), GuiStyle::FOREGROUND, text);
    }

    // Dibuja una tecla (o botón con texto) y devuelve su ancho.
    inline float keycap(ImDrawList* dl, const ImVec2& pos, const char* text,
                        float minWidth = KEY_SIZE, float height = KEY_SIZE) {
        const ImVec2 size = ImGui::CalcTextSize(text);
        const float width = (std::max)(minWidth, size.x + (height >= KEY_SIZE ? 20.0f : 14.0f));
        const ImVec2 corner(pos.x + width, pos.y + height);
        dl->AddRectFilled(pos, corner, GuiStyle::SURFACE, 6.0f);
        dl->AddRect(pos, corner, GuiStyle::ACCENT, 6.0f, 0, 2.0f);
        dl->AddText(ImVec2(pos.x + (width - size.x) * 0.5f, pos.y + (height - size.y) * 0.5f), GuiStyle::FOREGROUND, text);
        return width;
    }

    // Una o varias teclas seguidas de su descripción.
    inline void keyRow(ImDrawList* dl, const ImVec2& pos, std::initializer_list<const char*> keys,
                       const char* action, float minKeyWidth = KEY_SIZE) {
        float x = pos.x;
        for (const char* key : keys) x += keycap(dl, ImVec2(x, pos.y), key, minKeyWidth) + KEY_GAP;
        label(dl, x + 12.0f, pos.y + KEY_SIZE * 0.5f, action);
    }

    // Icono de ratón (36x54) con flechas de movimiento a los lados.
    inline void mouse(ImDrawList* dl, const ImVec2& pos) {
        const ImVec2 corner(pos.x + 36.0f, pos.y + 54.0f);
        dl->AddRectFilled(pos, corner, GuiStyle::SURFACE, 18.0f);
        dl->AddRect(pos, corner, GuiStyle::ACCENT, 18.0f, 0, 2.0f);
        dl->AddLine(ImVec2(pos.x, pos.y + 20.0f), ImVec2(corner.x, pos.y + 20.0f), GuiStyle::ACCENT, 2.0f);
        dl->AddLine(ImVec2(pos.x + 18.0f, pos.y), ImVec2(pos.x + 18.0f, pos.y + 20.0f), GuiStyle::ACCENT, 2.0f);
        dl->AddRectFilled(ImVec2(pos.x + 15.0f, pos.y + 6.0f), ImVec2(pos.x + 21.0f, pos.y + 14.0f), GuiStyle::ACCENT, 3.0f);

        const float centerY = pos.y + 27.0f;
        dl->AddTriangleFilled(ImVec2(pos.x - 14.0f, centerY), ImVec2(pos.x - 6.0f, centerY - 6.0f), ImVec2(pos.x - 6.0f, centerY + 6.0f), GuiStyle::FOREGROUND);
        dl->AddTriangleFilled(ImVec2(corner.x + 14.0f, centerY), ImVec2(corner.x + 6.0f, centerY - 6.0f), ImVec2(corner.x + 6.0f, centerY + 6.0f), GuiStyle::FOREGROUND);
    }

    // Botón frontal de un mando: símbolos de PlayStation o letras de colores de Xbox.
    // Los botones usados se rellenan; el resto queda apagado.
    inline void faceButton(ImDrawList* dl, const ImVec2& c, Face face, bool xboxStyle, bool used, float radius = 10.0f) {
        const int index = static_cast<int>(face);
        const float s = radius / 10.0f; // escala de los símbolos

        if (xboxStyle) {
            static constexpr const char* letters[] = { "Y", "B", "A", "X" };
            static constexpr ImU32 colors[] = {
                IM_COL32(240, 200, 40, 255), IM_COL32(230, 70, 70, 255),
                IM_COL32(70, 200, 100, 255), IM_COL32(70, 130, 235, 255),
            };
            dl->AddCircleFilled(c, radius, used ? colors[index] : GuiStyle::PANEL);
            if (!used) dl->AddCircle(c, radius, GuiStyle::MUTED, 0, 1.5f);
            const ImVec2 size = ImGui::CalcTextSize(letters[index]);
            dl->AddText(ImVec2(c.x - size.x * 0.5f, c.y - size.y * 0.5f), used ? GuiStyle::PANEL : GuiStyle::MUTED, letters[index]);
            return;
        }

        const ImU32 color = used ? GuiStyle::FOREGROUND : GuiStyle::MUTED;
        dl->AddCircleFilled(c, radius, used ? GuiStyle::ACCENT : GuiStyle::PANEL);
        if (!used) dl->AddCircle(c, radius, GuiStyle::MUTED, 0, 1.5f);
        switch (face) {
            case Face::NORTH: // triángulo
                dl->AddTriangle(ImVec2(c.x, c.y - 5.0f * s), ImVec2(c.x - 5.0f * s, c.y + 4.0f * s), ImVec2(c.x + 5.0f * s, c.y + 4.0f * s), color, 1.5f);
                break;
            case Face::EAST: // círculo
                dl->AddCircle(c, 5.0f * s, color, 0, 1.5f);
                break;
            case Face::SOUTH: // cruz
                dl->AddLine(ImVec2(c.x - 4.5f * s, c.y - 4.5f * s), ImVec2(c.x + 4.5f * s, c.y + 4.5f * s), color, 1.5f);
                dl->AddLine(ImVec2(c.x + 4.5f * s, c.y - 4.5f * s), ImVec2(c.x - 4.5f * s, c.y + 4.5f * s), color, 1.5f);
                break;
            case Face::WEST: // cuadrado
                dl->AddRect(ImVec2(c.x - 4.5f * s, c.y - 4.5f * s), ImVec2(c.x + 4.5f * s, c.y + 4.5f * s), color, 0.0f, 0, 1.5f);
                break;
        }
    }
}