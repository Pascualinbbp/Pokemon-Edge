#pragma once
#include "imgui.h"
#include "../gameState.hpp"
#include "../style/guiDraw.hpp"
#include "../style/guiLayout.hpp"
#include "../style/guiStyle.hpp"
#include "../window/guiInput.hpp"
#include "../../engine/core/inputDevice.hpp"

// Muestra únicamente los controles del dispositivo que se está usando.
namespace ControlsComponent {
    namespace detail {
        inline constexpr float PANEL_WIDTH = 760.0f;
        inline constexpr float PANEL_HEIGHT = 280.0f;

        // Posiciones (en el panel) de los elementos que cambian entre mandos.
        struct PadLayout {
            float lsX, lsY, lsElbowY; // stick izquierdo y altura de su línea indicadora
            float rsX, rsY, rsElbowY; // stick derecho
            float dpadX, dpadY;
            float faceX, faceY;       // centro del grupo de botones frontales
        };
        inline constexpr PadLayout PLAYSTATION_LAYOUT = { 322.0f, 150.0f, 262.0f, 438.0f, 150.0f, 262.0f, 292.0f, 102.0f, 468.0f, 102.0f };
        inline constexpr PadLayout XBOX_LAYOUT        = { 292.0f, 102.0f, 102.0f, 420.0f, 150.0f, 262.0f, 338.0f, 150.0f, 468.0f, 102.0f };

        inline constexpr const char* NOTES_KEYBOARD[] = {
            "Agacharse: pulsa SHIFT (otra vez para levantarte). Corriendo, SHIFT inicia un deslizamiento corto sin mantener la tecla.",
            "Durante el deslizamiento, A y D cambian la dirección. Saltar desde un deslizamiento conserva el impulso.",
            "Apuntar: clic derecho (otra vez o ESC para salir). Lanzar Pokéball: clic izquierdo mientras apuntas.",
            "TAB: fija la cámara al pokémon en rango (púlsalo otra vez para cambiar; mantenlo para soltar).",
            "Más captura: x1.5 por la espalda del pokémon y x2 si vas agachado sin que te detecte.",
            "La cámara orbita libremente alrededor del personaje; este solo la sigue al apuntar.",
        };
        inline constexpr const char* NOTES_GAMEPAD[] = {
            "Agacharse: pulsa el botón derecho (otra vez para levantarte). Corriendo (L3), inicia un deslizamiento corto.",
            "Durante el deslizamiento, inclina el stick izquierdo a un lado para girar. Saltar desde él conserva el impulso.",
            "Apuntar: mantén L2. Lanzar Pokéball: R2 mientras apuntas.",
            "R3: fija la cámara al pokémon en rango (púlsalo otra vez para cambiar; mantenlo para soltar).",
            "Más captura: x1.5 por la espalda del pokémon y x2 si vas agachado sin que te detecte.",
            "La cámara orbita libremente alrededor del personaje; este solo la sigue al apuntar.",
            "Menús: stick izquierdo o cruceta para moverte, botón inferior para aceptar y botón derecho para volver.",
        };

        inline const char* title(InputDevice device) {
            switch (device) {
                case InputDevice::XBOX:        return "CONTROLES: MANDO DE XBOX";
                case InputDevice::PLAYSTATION: return "CONTROLES: MANDO DE PLAYSTATION";
                default:                       return "CONTROLES: TECLADO Y RATÓN";
            }
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
            keyRow(dl, offset(o, 420.0f, 90.0f), { "ESC" }, "Pausa / Salir de apuntar", 60.0f);
            keyRow(dl, offset(o, 420.0f, 150.0f), { "TAB" }, "Fijar objetivo (mantener: soltar)", 60.0f);
            mouse(dl, offset(o, 440.0f, 204.0f));
            label(dl, o.x + 504.0f, o.y + 206.0f, "Mover la cámara");
            label(dl, o.x + 504.0f, o.y + 228.0f, "Der.: apuntar · Izq.: lanzar");
        }

        inline void drawGamepad(ImDrawList* dl, const ImVec2& o, InputDevice device) {
            using namespace GuiDraw;
            using namespace GuiStyle;
            const bool xbox = device == InputDevice::XBOX;
            const PadLayout& layout = xbox ? XBOX_LAYOUT : PLAYSTATION_LAYOUT;

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

            // Elementos centrales propios de cada mando; el botón de pausa (Options / Menú) está en uso.
            float pauseX, pauseY;
            if (xbox) {
                dl->AddCircle(P(380.0f, 86.0f), 9.0f, MUTED, 0, 2.0f);   // botón Xbox
                dl->AddCircle(P(350.0f, 102.0f), 5.0f, MUTED, 0, 1.5f);  // Ver
                dl->AddCircleFilled(P(410.0f, 102.0f), 5.0f, ACCENT);    // Menú
                pauseX = 410.0f;
                pauseY = 97.0f;
            } else {
                box(340.0f, 82.0f, 420.0f, 114.0f, 8.0f, PANEL, MUTED);  // panel táctil
                dl->AddRectFilled(P(428.0f, 86.0f), P(434.0f, 100.0f), ACCENT, 3.0f); // Options
                pauseX = 431.0f;
                pauseY = 86.0f;
            }

            // Cruceta (sin acción asignada en el juego; navega los menús).
            const float dx = layout.dpadX, dy = layout.dpadY;
            dl->AddRectFilled(P(dx - 19.0f, dy - 8.0f), P(dx + 19.0f, dy + 8.0f), MUTED, 3.0f);
            dl->AddRectFilled(P(dx - 8.0f, dy - 19.0f), P(dx + 8.0f, dy + 19.0f), MUTED, 3.0f);
            dl->AddRectFilled(P(dx - 17.0f, dy - 6.0f), P(dx + 17.0f, dy + 6.0f), PANEL, 3.0f);
            dl->AddRectFilled(P(dx - 6.0f, dy - 17.0f), P(dx + 6.0f, dy + 17.0f), PANEL, 3.0f);

            // Sticks (ambos tienen acción).
            for (const ImVec2 stick : { ImVec2(layout.lsX, layout.lsY), ImVec2(layout.rsX, layout.rsY) }) {
                dl->AddCircleFilled(P(stick.x, stick.y), 24.0f, PANEL);
                dl->AddCircle(P(stick.x, stick.y), 24.0f, ACCENT, 0, 2.0f);
                dl->AddCircleFilled(P(stick.x, stick.y), 15.0f, ACCENT);
            }

            // Botones frontales: se usan el inferior (saltar) y el derecho (agacharse).
            const float fx = layout.faceX, fy = layout.faceY;
            faceButton(dl, P(fx, fy - 17.0f), Face::NORTH, xbox, false);
            faceButton(dl, P(fx + 17.0f, fy), Face::EAST,  xbox, true);
            faceButton(dl, P(fx, fy + 17.0f), Face::SOUTH, xbox, true);
            faceButton(dl, P(fx - 17.0f, fy), Face::WEST,  xbox, false);

            // Líneas hacia cada acción (se dibujan al final para quedar encima).
            const auto callout = [&](float x, float y, float elbowY, float endX, const char* text) {
                dl->AddCircleFilled(P(x, y), 3.0f, FOREGROUND);
                dl->AddLine(P(x, y), P(x, elbowY), FOREGROUND, 1.5f);
                dl->AddLine(P(x, elbowY), P(endX, elbowY), FOREGROUND, 1.5f);
                const float textX = endX < x ? endX - 8.0f - ImGui::CalcTextSize(text).x : endX + 8.0f;
                label(dl, o.x + textX, o.y + elbowY, text);
            };
            callout(layout.lsX, layout.lsY, layout.lsElbowY, 210.0f, "Mover / Correr (pulsar L3)");
            callout(layout.rsX, layout.rsY, layout.rsElbowY, 550.0f, "Cámara · Fijar objetivo (R3)");
            callout(302.0f, 40.0f, 40.0f, 210.0f, "Apuntar (L2)");
            callout(457.0f, 40.0f, 40.0f, 550.0f, "Lanzar (R2)");
            callout(fx + 27.0f, fy, fy, 550.0f, "Agacharse / Deslizarse");
            callout(fx, fy + 27.0f, fy + 58.0f, 550.0f, "Saltar");
            callout(pauseX, pauseY, 14.0f, 550.0f, "Pausa");
        }
    }

    inline void render(GameState& state, InputDevice device) {
        GuiLayout::dimBackground();

        GuiLayout::beginAt(0.06f);
        GuiLayout::centeredText(detail::title(device));
        GuiLayout::gap(GuiLayout::GAP_SMALL);

        GuiLayout::centerX(detail::PANEL_WIDTH);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(detail::PANEL_WIDTH, detail::PANEL_HEIGHT));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(origin, GuiDraw::offset(origin, detail::PANEL_WIDTH, detail::PANEL_HEIGHT), GuiStyle::PANEL, 12.0f);
        if (isGamepad(device)) detail::drawGamepad(dl, origin, device);
        else detail::drawKeyboard(dl, origin);

        if (isGamepad(device)) for (const char* line : detail::NOTES_GAMEPAD) GuiLayout::centeredText(line, GuiStyle::MUTED);
        else for (const char* line : detail::NOTES_KEYBOARD) GuiLayout::centeredText(line, GuiStyle::MUTED);

        GuiLayout::gap(GuiLayout::GAP_SMALL);
        if (GuiInput::backButton()) state = GameState::PAUSED;
    }
}
