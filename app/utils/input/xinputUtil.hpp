#pragma once
#include <algorithm>
#include <cstdint>
#include <windows.h>
#include <xinput.h>
#include "gamepadState.hpp"
#pragma comment(lib, "xinput.lib")

// Mandos compatibles con XInput (Xbox 360/One/Series y emuladores como Steam Input o DS4Windows).
namespace XInputUtil {
    namespace detail {
        struct ButtonMap {
            WORD xinput;
            uint16_t button;
        };

        inline constexpr ButtonMap BUTTONS[] = {
            { XINPUT_GAMEPAD_A,              Gamepad::SOUTH },
            { XINPUT_GAMEPAD_B,              Gamepad::EAST },
            { XINPUT_GAMEPAD_X,              Gamepad::WEST },
            { XINPUT_GAMEPAD_Y,              Gamepad::NORTH },
            { XINPUT_GAMEPAD_LEFT_SHOULDER,  Gamepad::L1 },
            { XINPUT_GAMEPAD_RIGHT_SHOULDER, Gamepad::R1 },
            { XINPUT_GAMEPAD_LEFT_THUMB,     Gamepad::L3 },
            { XINPUT_GAMEPAD_RIGHT_THUMB,    Gamepad::R3 },
            { XINPUT_GAMEPAD_START,          Gamepad::START },
            { XINPUT_GAMEPAD_BACK,           Gamepad::BACK },
            { XINPUT_GAMEPAD_DPAD_UP,        Gamepad::DPAD_UP },
            { XINPUT_GAMEPAD_DPAD_DOWN,      Gamepad::DPAD_DOWN },
            { XINPUT_GAMEPAD_DPAD_LEFT,      Gamepad::DPAD_LEFT },
            { XINPUT_GAMEPAD_DPAD_RIGHT,     Gamepad::DPAD_RIGHT },
        };

        inline int slot = -1; // ranura del mando conectado, o -1

        inline float axis(SHORT value) {
            return (std::max)(static_cast<float>(value) / 32767.0f, -1.0f);
        }
    }

    inline bool connected() { return detail::slot >= 0; }

    // Busca el primer mando conectado. Preguntar por ranuras vacías es lento, así que solo se llama
    // cuando Windows avisa de un cambio de dispositivos (o de forma esporádica si no hay mando).
    inline bool rescan() {
        detail::slot = -1;
        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
            XINPUT_STATE state;
            if (XInputGetState(i, &state) == ERROR_SUCCESS) {
                detail::slot = static_cast<int>(i);
                break;
            }
        }
        return connected();
    }

    // Lee el mando conectado (una sola llamada). Devuelve false si se ha perdido.
    inline bool read(GamepadState& out) {
        XINPUT_STATE state;
        if (detail::slot < 0 || XInputGetState(static_cast<DWORD>(detail::slot), &state) != ERROR_SUCCESS) {
            detail::slot = -1;
            return false;
        }

        const XINPUT_GAMEPAD& pad = state.Gamepad;
        out = {};
        out.lx = detail::axis(pad.sThumbLX);
        out.ly = detail::axis(pad.sThumbLY);
        out.rx = detail::axis(pad.sThumbRX);
        out.ry = detail::axis(pad.sThumbRY);
        out.lt = pad.bLeftTrigger / 255.0f;
        out.rt = pad.bRightTrigger / 255.0f;
        for (const detail::ButtonMap& map : detail::BUTTONS) {
            if (pad.wButtons & map.xinput) out.buttons = static_cast<uint16_t>(out.buttons | map.button);
        }
        return true;
    }
}