#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiDraw.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiStyle.hpp"

namespace ControlsComponent {
    namespace detail {
        inline constexpr float PANEL_WIDTH = 760.0f;
        inline constexpr float PANEL_HEIGHT = 280.0f;
        inline constexpr float TAB_WIDTH = 210.0f;

        inline bool tabButton(const char* text, bool active) {
            if (!active) ImGui::PushStyleColor(ImGuiCol_Button, GuiStyle::TAB_INACTIVE);
            const bool clicked = ImGui::Button(text, ImVec2(TAB_WIDTH, 32.0f));
            if (!active) ImGui::PopStyleColor();
            return clicked;
        }

        inline void drawKeyboard(ImDrawList* dl, const ImVec2& o) {
            using namespace GuiDraw;
            const float step = KEY_SIZE + KEY_GAP;

            // Movimiento: W / A S D
            const ImVec2 move = offset(o, 40.0f, 30.0f);
            keycap(dl, offset(move, step, 0.0f), "W");
            keycap(dl, offset(move, 0.0f, step), "A");
            keycap(dl, offset(move, step, step), "S");
            keycap(dl, offset(move, 2.0f * step, step), "D");
            label(dl, move.x + 3.0f * step + 12.0f, move.y + step - KEY_GAP * 0.5f, "Mover");

            keyRow(dl, offset(o, 40.0f, 144.0f), { "W", "W" }, "Correr (pulsa W dos veces)");
            keyRow(dl, offset(o, 40.0f, 204.0f), { "SHIFT" }, "Agacharse / Deslizarse", 90.0f);

            keyRow(dl, offset(o, 420.0f, 30.0f), { "ESPACIO" }, "Saltar", 150.0f);
            keyRow(dl, offset(o, 420.0f, 90.0f), { "ESC" }, "Pausa / Volver", 60.0f);
            mouse(dl, offset(o, 440.0f, 150.0f));
            label(dl, o.x + 504.0f, o.y + 177.0f, "Mover la cámara");
        }

        inline void drawGamepad(ImDrawList* dl, const ImVec2& o) {
            using namespace GuiDraw;
            using namespace GuiStyle;
            const auto P = [&o](float x, float y) { return ImVec2(o.x + x, o.y + y); };
            const auto box = [&](float x0, float y0, float x1, float y1, float r, ImU32 fill, ImU32 border) {
                dl->AddRectFilled(P(x0, y0), P(x1, y1), fill, r);
                dl->AddRect(P(x0, y0), P(x1, y1), border, r, 0, 2.0f);
            };

            // Gatillos y botones superiores (quedan detrás del cuerpo).
            box(270.0f, 30.0f, 335.0f, 50.0f, 8.0f, SURFACE, MUTED); // L2
            box(425.0f, 30.0f, 490.0f, 50.0f, 8.0f, SURFACE, MUTED); // R2
            box(252.0f, 56.0f, 345.0f, 72.0f, 8.0f, SURFACE, MUTED); // L1
            box(415.0f, 56.0f, 508.0f, 72.0f, 8.0f, SURFACE, MUTED); // R1

            // Cuerpo y empuñaduras: primero los bordes ensanchados y luego los rellenos, así no se ven las uniones.
            struct Shape { float x0, y0, x1, y1, radius; };
            static constexpr Shape body[] = {
                { 240.0f,  68.0f, 520.0f, 186.0f, 50.0f },
                { 246.0f, 140.0f, 318.0f, 252.0f, 34.0f },
                { 442.0f, 140.0f, 514.0f, 252.0f, 34.0f },
            };
            for (const Shape& s : body) dl->AddRectFilled(P(s.x0 - 2.0f, s.y0 - 2.0f), P(s.x1 + 2.0f, s.y1 + 2.0f), MUTED, s.radius + 2.0f);
            for (const Shape& s : body) dl->AddRectFilled(P(s.x0, s.y0), P(s.x1, s.y1), SURFACE, s.radius);

            box(340.0f, 82.0f, 420.0f, 114.0f, 8.0f, PANEL, MUTED); // panel táctil

            // Cruceta (sin acción asignada).
            dl->AddRectFilled(P(273.0f, 94.0f), P(311.0f, 110.0f), MUTED, 3.0f);
            dl->AddRectFilled(P(284.0f, 83.0f), P(300.0f, 121.0f), MUTED, 3.0f);
            dl->AddRectFilled(P(275.0f, 96.0f), P(309.0f, 108.0f), PANEL, 3.0f);
            dl->AddRectFilled(P(286.0f, 85.0f), P(298.0f, 119.0f), PANEL, 3.0f);

            // Sticks (usados).
            for (const float x : { 322.0f, 438.0f }) {
                dl->AddCircleFilled(P(x, 150.0f), 24.0f, PANEL);
                dl->AddCircle(P(x, 150.0f), 24.0f, ACCENT, 0, 2.0f);
                dl->AddCircleFilled(P(x, 150.0f), 15.0f, ACCENT);
            }

            // Botones frontales: los usados se rellenan con el color de acento.
            const auto face = [&](float x, float y, FaceSymbol symbol, bool used) {
                dl->AddCircleFilled(P(x, y), 10.0f, used ? ACCENT : PANEL);
                if (!used) dl->AddCircle(P(x, y), 10.0f, MUTED, 0, 1.5f);
                faceSymbol(dl, P(x, y), symbol, used ? FOREGROUND : MUTED);
            };
            face(468.0f,  85.0f, FaceSymbol::TRIANGLE, false);
            face(485.0f, 102.0f, FaceSymbol::CIRCLE,   true);
            face(468.0f, 119.0f, FaceSymbol::CROSS,    true);
            face(451.0f, 102.0f, FaceSymbol::SQUARE,   false);

            dl->AddRectFilled(P(428.0f, 86.0f), P(434.0f, 100.0f), ACCENT, 3.0f); // Options

            // Líneas hacia cada acción (se dibujan al final para quedar encima).
            const auto callout = [&](float x, float y, float elbowY, float endX, const char* text) {
                dl->AddCircleFilled(P(x, y), 3.0f, FOREGROUND);
                dl->AddLine(P(x, y), P(x, elbowY), FOREGROUND, 1.5f);
                dl->AddLine(P(x, elbowY), P(endX, elbowY), FOREGROUND, 1.5f);
                const float textX = endX < x ? endX - 8.0f - ImGui::CalcTextSize(text).x : endX + 8.0f;
                label(dl, o.x + textX, o.y + elbowY, text);
            };
            callout(322.0f, 150.0f, 262.0f, 210.0f, "Mover / Correr (pulsar)");
            callout(438.0f, 150.0f, 262.0f, 550.0f, "Mover la cámara");
            callout(495.0f, 102.0f, 102.0f, 550.0f, "Agacharse / Deslizarse");
            callout(468.0f, 129.0f, 160.0f, 550.0f, "Saltar");
            callout(431.0f,  86.0f,  14.0f, 550.0f, "Pausa");
        }
    }

    inline void render(GameState& state) {
        static bool gamepadTab = false;

        GuiLayout::dimBackground();

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y * 0.06f);
        GuiLayout::centeredText("CONTROLES");

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        GuiLayout::centerX(2.0f * detail::TAB_WIDTH + ImGui::GetStyle().ItemSpacing.x);
        if (detail::tabButton("TECLADO Y RATÓN", !gamepadTab)) gamepadTab = false;
        ImGui::SameLine();
        if (detail::tabButton("MANDO", gamepadTab)) gamepadTab = true;

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        GuiLayout::centerX(detail::PANEL_WIDTH);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(detail::PANEL_WIDTH, detail::PANEL_HEIGHT));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(origin, GuiDraw::offset(origin, detail::PANEL_WIDTH, detail::PANEL_HEIGHT), GuiStyle::PANEL, 12.0f);
        if (gamepadTab) detail::drawGamepad(dl, origin);
        else detail::drawKeyboard(dl, origin);

        ImGui::PushStyleColor(ImGuiCol_Text, GuiStyle::MUTED);
        GuiLayout::centeredText(gamepadTab
            ? "Disposición prevista: el soporte de mando se añadirá más adelante."
            : "Deslizarse: corre (W W) y pulsa SHIFT, o salta corriendo y agáchate en el aire.");
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        if (GuiLayout::centeredButton("VOLVER") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            state = GameState::PAUSED;
        }
    }
}