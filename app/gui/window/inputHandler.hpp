#pragma once
#include <windows.h>
#include "../../engine/core/input.hpp"

// Entrada de la plataforma (Win32): teclado y ratón (Raw Input) para el juego.
namespace InputHandler {
    namespace detail {
        inline constexpr DWORD DOUBLE_TAP_MS = 300;
        inline constexpr LPARAM REPEAT_FLAG = 1 << 30;      // bit 30 de lParam: la tecla ya estaba pulsada
        inline constexpr UINT LEFT_SHIFT_SCANCODE = 0x2A;

        inline InputState state;     // teclas mantenidas + eventos de un frame + delta de ratón acumulado
        inline bool captured = false;
        inline DWORD lastWPress = 0;

        // Descarta lo que no debe arrastrarse entre frames o entre estados (pausa, carga...).
        inline void clearEvents() {
            state.mouseDX = state.mouseDY = 0.0f;
            state.jump = state.sprint = false;
        }

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

    inline void onKey(WPARAM key, LPARAM lParam, bool pressed) {
        InputState& s = detail::state;
        const bool newPress = pressed && !(lParam & detail::REPEAT_FLAG); // ignora la repetición automática

        switch (key) {
            case 'W':
                if (newPress) {
                    const DWORD now = GetMessageTime();
                    if (now - detail::lastWPress <= detail::DOUBLE_TAP_MS) {
                        s.sprint = true;
                        detail::lastWPress = 0; // un tercer toque no cuenta como otro doble toque
                    } else {
                        detail::lastWPress = now;
                    }
                }
                s.up = pressed;
                break;
            case 'S': s.down = pressed; break;
            case 'A': s.left = pressed; break;
            case 'D': s.right = pressed; break;
            case VK_SPACE:
                if (newPress) s.jump = true;
                break;
            case VK_SHIFT: // solo el shift izquierdo (por su scancode)
                if (((lParam >> 16) & 0xFF) == detail::LEFT_SHIFT_SCANCODE) s.crouch = pressed;
                break;
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
        detail::clearEvents();

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

    // Estado de entrada de este frame; consume los eventos y el delta del ratón acumulados.
    inline InputState poll() {
        const InputState snapshot = detail::state;
        detail::clearEvents();
        return snapshot;
    }
}