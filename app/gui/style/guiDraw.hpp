#pragma once
#include <algorithm>
#include <initializer_list>
#include "imgui.h"
#include "guiStyle.hpp"

// Primitivas de dibujo con ImDrawList (teclas, ratón, símbolos de mando).
namespace GuiDraw {
    inline constexpr float KEY_SIZE = 40.0f;
    inline constexpr float KEY_GAP = 4.0f;

    enum class FaceSymbol { TRIANGLE, CIRCLE, CROSS, SQUARE };

    inline ImVec2 offset(const ImVec2& origin, float x, float y) {
        return ImVec2(origin.x + x, origin.y + y);
    }

    // Texto cuya línea vertical queda centrada en y.
    inline void label(ImDrawList* dl, float x, float y, const char* text) {
        dl->AddText(ImVec2(x, y - ImGui::GetTextLineHeight() * 0.5f), GuiStyle::FOREGROUND, text);
    }

    // Dibuja una tecla y devuelve su ancho.
    inline float keycap(ImDrawList* dl, const ImVec2& pos, const char* text, float minWidth = KEY_SIZE) {
        const ImVec2 size = ImGui::CalcTextSize(text);
        const float width = (std::max)(minWidth, size.x + 20.0f);
        const ImVec2 max(pos.x + width, pos.y + KEY_SIZE);
        dl->AddRectFilled(pos, max, GuiStyle::SURFACE, 6.0f);
        dl->AddRect(pos, max, GuiStyle::ACCENT, 6.0f, 0, 2.0f);
        dl->AddText(ImVec2(pos.x + (width - size.x) * 0.5f, pos.y + (KEY_SIZE - size.y) * 0.5f), GuiStyle::FOREGROUND, text);
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
        const ImVec2 max(pos.x + 36.0f, pos.y + 54.0f);
        dl->AddRectFilled(pos, max, GuiStyle::SURFACE, 18.0f);
        dl->AddRect(pos, max, GuiStyle::ACCENT, 18.0f, 0, 2.0f);
        dl->AddLine(ImVec2(pos.x, pos.y + 20.0f), ImVec2(max.x, pos.y + 20.0f), GuiStyle::ACCENT, 2.0f);
        dl->AddLine(ImVec2(pos.x + 18.0f, pos.y), ImVec2(pos.x + 18.0f, pos.y + 20.0f), GuiStyle::ACCENT, 2.0f);
        dl->AddRectFilled(ImVec2(pos.x + 15.0f, pos.y + 6.0f), ImVec2(pos.x + 21.0f, pos.y + 14.0f), GuiStyle::ACCENT, 3.0f);

        const float cy = pos.y + 27.0f;
        dl->AddTriangleFilled(ImVec2(pos.x - 14.0f, cy), ImVec2(pos.x - 6.0f, cy - 6.0f), ImVec2(pos.x - 6.0f, cy + 6.0f), GuiStyle::FOREGROUND);
        dl->AddTriangleFilled(ImVec2(max.x + 14.0f, cy), ImVec2(max.x + 6.0f, cy - 6.0f), ImVec2(max.x + 6.0f, cy + 6.0f), GuiStyle::FOREGROUND);
    }

    // Símbolo de un botón frontal del mando, centrado en c.
    inline void faceSymbol(ImDrawList* dl, const ImVec2& c, FaceSymbol symbol, ImU32 color) {
        switch (symbol) {
            case FaceSymbol::TRIANGLE:
                dl->AddTriangle(ImVec2(c.x, c.y - 5.0f), ImVec2(c.x - 5.0f, c.y + 4.0f), ImVec2(c.x + 5.0f, c.y + 4.0f), color, 1.5f);
                break;
            case FaceSymbol::CIRCLE:
                dl->AddCircle(c, 5.0f, color, 0, 1.5f);
                break;
            case FaceSymbol::CROSS:
                dl->AddLine(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), color, 1.5f);
                dl->AddLine(ImVec2(c.x + 4.5f, c.y - 4.5f), ImVec2(c.x - 4.5f, c.y + 4.5f), color, 1.5f);
                break;
            case FaceSymbol::SQUARE:
                dl->AddRect(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), color, 0.0f, 0, 1.5f);
                break;
        }
    }
}