#pragma once
#include <windows.h>
#include "../../engine/core/input.hpp"

// Entrada de la plataforma (Win32): teclado y ratón (Raw Input) para el juego.
namespace InputHandler {
    namespace detail {
        inline InputState state;     // teclas pulsadas + delta de ratón acumulado
        inline bool captured = false;

        // Centra el cursor y lo encierra en un rectángulo de 1 píxel (queda inmóvil y oculto).
        inline void lockCursor(HWND hwnd) {
            RECT client;
            GetClientRect(hwnd, &client);
            POINT center = { client.right / 2, client.bottom / 2 };
            ClientToScreen(hwnd, &center);
            SetCursorPos(center.x, center.y);
            const RECT lock = { center.x, center.y, center.x + 1, center.y + 1 };
            ClipCursor(&lock);
        }
    }

    inline void onKey(WPARAM key, bool pressed) {
        switch (key) {
            case 'W':      detail::state.up = pressed; break;
            case 'S':      detail::state.down = pressed; break;
            case 'A':      detail::state.left = pressed; break;
            case 'D':      detail::state.right = pressed; break;
            case VK_SPACE: detail::state.jump = pressed; break;
        }
    }

    // Movimiento relativo del ratón (WM_INPUT). Solo llega mientras hay captura.
    inline void onRawInput(LPARAM lParam) {
        RAWINPUT raw;
        UINT size = sizeof(raw);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) return;
        if (raw.header.dwType != RIM_TYPEMOUSE || (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) return;

        detail::state.mouseDX += static_cast<float>(raw.data.mouse.lLastX);
        detail::state.mouseDY += static_cast<float>(raw.data.mouse.lLastY);
    }

    // Evita teclas "pegadas" al perder el foco.
    inline void resetKeys() { detail::state = InputState{}; }

    // Si la ventana se mueve o cambia de tamaño durante la captura, recoloca el bloqueo.
    inline void relockCursor(HWND hwnd) {
        if (detail::captured) detail::lockCursor(hwnd);
    }

    // Oculta y bloquea el cursor y activa Raw Input solo mientras se juega. Es idempotente.
    inline void setMouseCapture(HWND hwnd, bool capture) {
        if (capture == detail::captured) return;
        detail::captured = capture;
        detail::state.mouseDX = detail::state.mouseDY = 0.0f;

        const RAWINPUTDEVICE mouse = { 0x01, 0x02, capture ? 0u : static_cast<DWORD>(RIDEV_REMOVE), capture ? hwnd : nullptr };
        RegisterRawInputDevices(&mouse, 1, sizeof(mouse));

        if (capture) {
            ShowCursor(FALSE);
            detail::lockCursor(hwnd);
        } else {
            ClipCursor(nullptr);
            ShowCursor(TRUE);
        }
    }

    // Estado de entrada de este frame; consume el delta del ratón acumulado.
    inline InputState poll() {
        const InputState snapshot = detail::state;
        detail::state.mouseDX = detail::state.mouseDY = 0.0f;
        return snapshot;
    }
}