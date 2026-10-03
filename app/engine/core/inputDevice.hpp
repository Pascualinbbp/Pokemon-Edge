#pragma once

// Dispositivo con el que se está jugando; decide qué controles se muestran.
enum class InputDevice {
    KEYBOARD_MOUSE,
    XBOX,
    PLAYSTATION
};

constexpr bool isGamepad(InputDevice device) {
    return device != InputDevice::KEYBOARD_MOUSE;
}