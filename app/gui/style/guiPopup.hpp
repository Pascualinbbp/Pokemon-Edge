#pragma once
#include <vector>
#include "imgui.h"
#include "guiPrompts.hpp"
#include "../../engine/core/promptAction.hpp"

// Popups del juego: una pastilla translúcida con el botón a pulsar y lo que hace, o un simple aviso de texto.
// Es lo único que dibuja ayudas flotantes: interactuar con un recurso, pescar (Capturar / Liberar)..
namespace GuiPopup {
    inline constexpr float PAD_X = 12.0f, PAD_Y = 6.0f, GAP = 8.0f, ROUNDING = 10.0f;

    inline void text(ImDrawList* dl, ImVec2 center, const char* label, float scale, ImU32 color) {
        ImFont* font = ImGui::GetFont();
        const float size = ImGui::GetFontSize() * scale;
        const ImVec2 extent = font->CalcTextSizeA(size, 1.0e9f, 0.0f, label);
        const ImVec2 pos(center.x - extent.x * 0.5f, center.y);
        dl->AddText(font, size, ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 200), label);
        dl->AddText(font, size, pos, color, label);
    }

    // Ancho de una pastilla con ese texto.
    inline float width(const char* label) { return PAD_X * 2.0f + 24.0f + 8.0f + ImGui::CalcTextSize(label).x + 4.0f; }

    // Una pastilla con su botón y su texto, con la esquina superior izquierda en 'pos'. Devuelve su ancho.
    inline float pill(ImDrawList* dl, ImVec2 pos, GuiPrompts::Action action, const char* label, InputDevice device, bool enabled = true) {
        const float w = width(label), h = 24.0f + PAD_Y * 2.0f;
        dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(12, 12, 18, enabled ? 190 : 150), ROUNDING);
        dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(255, 255, 255, enabled ? 70 : 35), ROUNDING, 0, 1.5f);
        GuiPrompts::draw(dl, ImVec2(pos.x + PAD_X, pos.y + PAD_Y), action, label, device);
        if (!enabled) dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(0, 0, 0, 110), ROUNDING);
        return w;
    }

    // Una pastilla centrada en 'center' (por ejemplo sobre un objeto del mundo).
    inline void at(ImDrawList* dl, ImVec2 center, GuiPrompts::Action action, const char* label, InputDevice device, bool enabled = true) {
        pill(dl, ImVec2(center.x - width(label) * 0.5f, center.y - (24.0f + PAD_Y * 2.0f) * 0.5f), action, label, device, enabled);
    }

    // Varias pastillas lado a lado, centradas en x y con su borde superior en y (Capturar | Liberar).
    inline void row(ImDrawList* dl, float centerX, float y, const std::vector<Prompt>& prompts, InputDevice device) {
        float total = -12.0f;
        for (const Prompt& p : prompts) total += width(p.text.c_str()) + 12.0f;
        float x = centerX - total * 0.5f;
        for (const Prompt& p : prompts) x += pill(dl, ImVec2(x, y), p.action, p.text.c_str(), device, p.enabled) + 12.0f;
    }
}
