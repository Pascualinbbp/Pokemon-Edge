#pragma once
#include <windows.h>

// Operaciones genéricas con ventanas Win32.
namespace WindowUtil {
    // Los informes de los mandos (WM_INPUT) no deben despertar la espera pasiva del bucle principal.
    inline constexpr DWORD IDLE_WAKE_MASK = QS_ALLINPUT & ~QS_RAWINPUT;

    struct Size {
        int width;
        int height;
    };

    inline Size clientSize(HWND hwnd) {
        RECT client;
        GetClientRect(hwnd, &client);
        return { client.right, client.bottom };
    }

    // Registra la clase de ventana y crea una ventana normal con borde.
    inline HWND create(const char* className, const char* title, WNDPROC procedure, int width, int height,
                       int x = 100, int y = 100) {
        WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, procedure, 0L, 0L, GetModuleHandle(nullptr),
            nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, className, nullptr };
        RegisterClassEx(&wc);
        return CreateWindowEx(0, className, title, WS_OVERLAPPEDWINDOW, x, y, width, height,
            nullptr, nullptr, wc.hInstance, nullptr);
    }

    inline void setIcon(HWND hwnd, HICON icon) {
        if (!icon) return;
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }

    // Espera pasiva: duerme hasta que llegue un mensaje de ventana o pase 'timeoutMs' (INFINITE = solo mensajes).
    inline void waitForMessages(DWORD timeoutMs) {
        MsgWaitForMultipleObjectsEx(0, nullptr, timeoutMs, IDLE_WAKE_MASK, MWMO_INPUTAVAILABLE);
    }

    // Pantalla completa sin bordes; al salir se restaura la posición y el tamaño anteriores.
    class Fullscreen {
        public:
        void set(HWND hwnd, bool enable) {
            if (enable == m_active) return;
            m_active = enable;

            if (enable) {
                MONITORINFO monitor = { sizeof(monitor) };
                GetWindowPlacement(hwnd, &m_placement);
                GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
                SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                SetWindowPos(hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                    monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                    SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
            } else {
                SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
                SetWindowPlacement(hwnd, &m_placement);
                SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
        }

        private:
        bool m_active = false;
        WINDOWPLACEMENT m_placement = { sizeof(WINDOWPLACEMENT) };
    };
}