#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Gamepad {
    // Botones por posición, no por nombre: SOUTH es A en Xbox y ✕ en PlayStation, etc.
    enum Button : uint16_t {
        SOUTH      = 1 << 0,
        EAST       = 1 << 1,
        WEST       = 1 << 2,
        NORTH      = 1 << 3,
        L1         = 1 << 4,
        R1         = 1 << 5,
        L3         = 1 << 6,
        R3         = 1 << 7,
        START      = 1 << 8,
        BACK       = 1 << 9,
        DPAD_UP    = 1 << 10,
        DPAD_DOWN  = 1 << 11,
        DPAD_LEFT  = 1 << 12,
        DPAD_RIGHT = 1 << 13,
    };
}

// Estado normalizado de un mando, independiente del fabricante.
struct GamepadState {
    // Sticks de -1 a 1: derecha y arriba son positivos.
    float lx = 0.0f, ly = 0.0f;
    float rx = 0.0f, ry = 0.0f;
    // Gatillos de 0 a 1.
    float lt = 0.0f, rt = 0.0f;
    uint16_t buttons = 0;

    // Con las zonas muertas ya aplicadas, indica si el jugador está tocando el mando.
    bool hasActivity() const {
        return buttons != 0 || lx != 0.0f || ly != 0.0f || rx != 0.0f || ry != 0.0f || lt > 0.2f || rt > 0.2f;
    }
};

// Zona muerta radial: ignora el ruido cerca del centro y reescala el resto a 0..1.
inline void applyDeadzone(float& x, float& y, float deadzone) {
    const float length = std::sqrt(x * x + y * y);
    if (length <= deadzone) {
        x = y = 0.0f;
        return;
    }
    const float scale = (std::min)((length - deadzone) / (1.0f - deadzone), 1.0f) / length;
    x *= scale;
    y *= scale;
}