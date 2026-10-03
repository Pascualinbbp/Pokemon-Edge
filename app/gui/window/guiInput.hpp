#pragma once
#include <algorithm>
#include <cstdint>
#include "imgui.h"
#include "inputHandler.hpp"
#include "../../utils/input/gamepadState.hpp"

// Puente entre el mando y la interfaz: alimenta la navegación de ImGui (stick izquierdo / cruceta para
// moverse, botón inferior para aceptar) y detecta las pulsaciones de "volver" y "pausa".
// Funciona con cualquier mando porque lee el GamepadState normalizado, no el de ImGui/XInput.
namespace GuiInput {
    namespace detail {
        inline GamepadState pad;          // última lectura (con las teclas mantenidas desde antes ya filtradas)
        inline bool padOk = false;
        inline uint16_t suppressed = 0;   // botones mantenidos antes de empezar a leer: se ignoran hasta soltarlos
        inline uint16_t pressedEdges = 0; // pulsaciones nuevas desde el último frame dibujado

        inline void analog(ImGuiIO& io, ImGuiKey key, float value) {
            const float amount = (std::max)(value, 0.0f);
            io.AddKeyAnalogEvent(key, amount > 0.1f, amount);
        }

        // Escribe el estado completo del mando en ImGui; ImGui descarta lo que no cambia.
        inline void write(ImGuiIO& io, const GamepadState& p) {
            struct ButtonKey { ImGuiKey key; uint16_t mask; };
            static constexpr ButtonKey BUTTONS[] = {
                { ImGuiKey_GamepadFaceDown,  Gamepad::SOUTH },
                { ImGuiKey_GamepadFaceRight, Gamepad::EAST },
                { ImGuiKey_GamepadFaceLeft,  Gamepad::WEST },
                { ImGuiKey_GamepadFaceUp,    Gamepad::NORTH },
                { ImGuiKey_GamepadL1,        Gamepad::L1 },
                { ImGuiKey_GamepadR1,        Gamepad::R1 },
                { ImGuiKey_GamepadL3,        Gamepad::L3 },
                { ImGuiKey_GamepadR3,        Gamepad::R3 },
                { ImGuiKey_GamepadStart,     Gamepad::START },
                { ImGuiKey_GamepadBack,      Gamepad::BACK },
                { ImGuiKey_GamepadDpadUp,    Gamepad::DPAD_UP },
                { ImGuiKey_GamepadDpadDown,  Gamepad::DPAD_DOWN },
                { ImGuiKey_GamepadDpadLeft,  Gamepad::DPAD_LEFT },
                { ImGuiKey_GamepadDpadRight, Gamepad::DPAD_RIGHT },
            };
            for (const ButtonKey& button : BUTTONS) io.AddKeyEvent(button.key, (p.buttons & button.mask) != 0);

            analog(io, ImGuiKey_GamepadLStickLeft,  -p.lx);
            analog(io, ImGuiKey_GamepadLStickRight,  p.lx);
            analog(io, ImGuiKey_GamepadLStickUp,     p.ly);
            analog(io, ImGuiKey_GamepadLStickDown,  -p.ly);
            analog(io, ImGuiKey_GamepadRStickLeft,  -p.rx);
            analog(io, ImGuiKey_GamepadRStickRight,  p.rx);
            analog(io, ImGuiKey_GamepadRStickUp,     p.ry);
            analog(io, ImGuiKey_GamepadRStickDown,  -p.ry);
            analog(io, ImGuiKey_GamepadL2, p.lt);
            analog(io, ImGuiKey_GamepadR2, p.rt);
        }
    }

    // Ignora los botones que ya estén pulsados hasta que se suelten. Llamar al salir del juego: el botón
    // que pausa (Options) no debe contar además como "continuar" en el menú.
    inline void suppress() { detail::suppressed = 0xFFFF; }

    // Suelta todo en ImGui. Llamar al volver al juego, donde el mando lo lee InputHandler::poll.
    inline void release() {
        detail::pad = {};
        detail::padOk = false;
        detail::pressedEdges = 0;
        detail::write(ImGui::GetIO(), detail::pad);
    }

    // Lee el mando (una sola lectura). Devuelve true si la interfaz debe redibujarse: algo cambió
    // o hay algo pulsado (para que ImGui repita el movimiento mientras se mantiene el stick).
    inline bool pollChanged() {
        GamepadState current;
        const bool ok = InputHandler::readGamepad(current);
        if (!ok) current = {};

        detail::suppressed &= current.buttons;
        current.buttons = static_cast<uint16_t>(current.buttons & ~detail::suppressed);

        const GamepadState& previous = detail::pad;
        const bool changed = current.buttons != previous.buttons ||
            current.lx != previous.lx || current.ly != previous.ly ||
            current.rx != previous.rx || current.ry != previous.ry;

        detail::pressedEdges = static_cast<uint16_t>(detail::pressedEdges | (current.buttons & ~previous.buttons));
        detail::pad = current;
        detail::padOk = ok;
        return ok && (changed || current.hasActivity());
    }

    // Vuelca el último estado a ImGui. Debe llamarse después de ImGui_ImplWin32_NewFrame (que toca las
    // marcas de mando del backend) y antes de ImGui::NewFrame.
    inline void feed() {
        ImGuiIO& io = ImGui::GetIO();
        if (detail::padOk) io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
        detail::write(io, detail::pad);
    }

    // Al terminar de dibujar el frame, las pulsaciones nuevas ya se han consumido.
    inline void endFrame() { detail::pressedEdges = 0; }

    // "Volver": ESC o el botón derecho del mando (○ / B).
    inline bool backPressed() {
        return ImGui::IsKeyPressed(ImGuiKey_Escape, false) || (detail::pressedEdges & Gamepad::EAST) != 0;
    }

    // Options (PlayStation) / Menú (Xbox).
    inline bool startPressed() { return (detail::pressedEdges & Gamepad::START) != 0; }

    inline bool anyButtonPressed() { return detail::pressedEdges != 0; }
}