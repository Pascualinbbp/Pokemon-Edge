#pragma once
#include <cstdint>
#include "../../utils/input/gamepadState.hpp"

// Asignación de acciones del mando. Los botones se indican por posición (equivale en cualquier mando).
// Para añadir una acción nueva: constante aquí + campo en InputState + mapeo en InputHandler.
namespace InputBindings {
    inline constexpr uint16_t PAD_JUMP   = Gamepad::SOUTH; // ✕ en PlayStation, A en Xbox
    inline constexpr uint16_t PAD_CROUCH = Gamepad::EAST;  // ○ en PlayStation, B en Xbox
    inline constexpr uint16_t PAD_SPRINT = Gamepad::L3;    // pulsar el stick izquierdo
    inline constexpr uint16_t PAD_PAUSE  = Gamepad::START; // Options en PlayStation, Menú en Xbox

    inline constexpr float STICK_DEADZONE = 0.2f;
    inline constexpr float LOOK_SPEED = 1000.0f; // "píxeles" por segundo con el stick derecho a fondo
}