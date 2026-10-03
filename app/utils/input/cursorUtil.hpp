#pragma once
#include <windows.h>

// Cursor del sistema: ocultarlo y bloquearlo para controlar la cámara con el ratón.
// Las llamadas no se acumulan por sí solas: quien las usa debe hacerlo en pares (capture / release).
namespace CursorUtil {
    // Centra el cursor y lo encierra en un rectángulo de 1 píxel (queda inmóvil).
    inline void lockToCenter(HWND hwnd) {
        RECT client;
        GetClientRect(hwnd, &client);
        POINT center = { client.right / 2, client.bottom / 2 };
        ClientToScreen(hwnd, &center);
        SetCursorPos(center.x, center.y);
        const RECT lock = { center.x, center.y, center.x + 1, center.y + 1 };
        ClipCursor(&lock);
    }

    inline void capture(HWND hwnd) {
        ShowCursor(FALSE);
        lockToCenter(hwnd);
    }

    inline void release() {
        ClipCursor(nullptr);
        ShowCursor(TRUE);
    }
}