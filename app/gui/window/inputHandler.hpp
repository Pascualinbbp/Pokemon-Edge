#pragma once
#include <windows.h>
#include "../../engine/core/input.hpp"

// Entrada de la plataforma (Win32): estado del teclado y captura del ratón para la cámara.
namespace InputHandler {
    namespace detail {
        inline InputState keys;
        inline bool captured = false;

        // Devuelve el centro del área cliente en coordenadas de pantalla y el rectángulo del área cliente.
        inline POINT clientArea(HWND hwnd, RECT& area) {
            RECT client;
            GetClientRect(hwnd, &client);
            POINT topLeft = { client.left, client.top };
            POINT bottomRight = { client.right, client.bottom };
            ClientToScreen(hwnd, &topLeft);
            ClientToScreen(hwnd, &bottomRight);
            area = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
            return { (topLeft.x + bottomRight.x) / 2, (topLeft.y + bottomRight.y) / 2 };
        }
    }

    inline void onKey(WPARAM key, bool pressed) {
        switch (key) {
            case 'W': detail::keys.up = pressed; break;
            case 'S': detail::keys.down = pressed; break;
            case 'A': detail::keys.left = pressed; break;
            case 'D': detail::keys.right = pressed; break;
        }
    }

    // Evita teclas "pegadas" al perder el foco.
    inline void resetKeys() { detail::keys = InputState{}; }

    // Oculta y bloquea el cursor en la ventana mientras se juega. Es idempotente.
    inline void setMouseCapture(HWND hwnd, bool capture) {
        if (capture == detail::captured) return;
        detail::captured = capture;

        if (capture) {
            RECT area;
            const POINT center = detail::clientArea(hwnd, area);
            ShowCursor(FALSE);
            ClipCursor(&area);
            SetCursorPos(center.x, center.y); // el primer delta parte de 0
        } else {
            ClipCursor(nullptr);
            ShowCursor(TRUE);
        }
    }

    // Estado de entrada de este frame: teclas pulsadas + movimiento del ratón desde el centro.
    inline InputState poll(HWND hwnd) {
        InputState state = detail::keys;
        if (detail::captured && GetForegroundWindow() == hwnd) {
            RECT area;
            const POINT center = detail::clientArea(hwnd, area);
            ClipCursor(&area); // se mantiene al día si la ventana se mueve o cambia de tamaño

            POINT p;
            if (GetCursorPos(&p)) {
                state.mouseDX = static_cast<float>(p.x - center.x);
                state.mouseDY = static_cast<float>(p.y - center.y);
                SetCursorPos(center.x, center.y);
            }
        }
        return state;
    }
}