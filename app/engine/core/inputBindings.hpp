#pragma once
#include <cstdint>
#include "../../utils/input/gamepadState.hpp"

// Asignación de acciones del mando. Los botones se indican por posición (equivale en cualquier mando).
// Para añadir una acción nueva: constante aquí + campo en InputState + mapeo en InputHandler.
namespace InputBindings {
    inline constexpr uint16_t PAD_JUMP   = Gamepad::SOUTH; // ✕ en PlayStation, A en Xbox
    inline constexpr uint16_t PAD_CROUCH = Gamepad::EAST;  // ○ en PlayStation, B en Xbox
    inline constexpr uint16_t PAD_SPRINT = Gamepad::L3;    // pulsar el stick izquierdo
    inline constexpr uint16_t PAD_LOCK   = Gamepad::R3;    // pulsar el stick derecho: fijar / cambiar objetivo
    inline constexpr uint16_t PAD_PAUSE  = Gamepad::START; // Options en PlayStation, Menú en Xbox
    inline constexpr uint16_t PAD_INTERACT = Gamepad::WEST; // cuadrado en PlayStation, X en Xbox: abrir cofres
    inline constexpr uint16_t PAD_INVENTORY = Gamepad::BACK; // Select en PlayStation, Ver en Xbox: inventario
    inline constexpr uint16_t PAD_MAP = Gamepad::DPAD_UP;   // cruceta arriba: mapa
    inline constexpr uint16_t PAD_POKEDEX = Gamepad::DPAD_RIGHT; // cruceta derecha: Pokédex
    inline constexpr uint16_t PAD_MODE = Gamepad::NORTH;   // triángulo en PlayStation, Y en Xbox: modo del pokémon
    inline constexpr uint16_t PAD_BALL_PREV = Gamepad::L1; // pokéball anterior
    inline constexpr uint16_t PAD_BALL_NEXT = Gamepad::R1; // pokéball siguiente

    // Gatillos analógicos: L2 mantenido = apuntar; R2 (al cruzar el umbral) = lanzar.
    inline constexpr float TRIGGER_THRESHOLD = 0.5f;

    inline constexpr unsigned long LOCK_HOLD_MS = 400; // mantener TAB / R3 este tiempo = soltar el objetivo

    inline constexpr float STICK_DEADZONE = 0.2f;
    inline constexpr float LOOK_SPEED = 1000.0f; // "píxeles" por segundo con el stick derecho a fondo
}
