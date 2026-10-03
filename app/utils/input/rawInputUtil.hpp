#pragma once
#include <windows.h>

// Raw Input de Windows: alta precisión para el ratón y informes HID de los mandos.
namespace RawInputUtil {
    inline constexpr USHORT USAGE_PAGE_GENERIC_DESKTOP = 0x01;
    inline constexpr USHORT USAGE_MOUSE = 0x02;
    inline constexpr USHORT USAGE_GAMEPAD = 0x05;

    // Pide (o deja de pedir) a Windows los informes de un tipo de dispositivo para esta ventana.
    inline void registerDevice(HWND hwnd, USHORT usage, bool enable) {
        const RAWINPUTDEVICE device = { USAGE_PAGE_GENERIC_DESKTOP, usage,
            enable ? 0u : static_cast<DWORD>(RIDEV_REMOVE), enable ? hwnd : nullptr };
        RegisterRawInputDevices(&device, 1, sizeof(device));
    }

    // Lee un mensaje WM_INPUT en un buffer reutilizable (los informes HID varían de tamaño).
    // El puntero solo es válido hasta la siguiente llamada. Devuelve nullptr si no se puede leer.
    inline const RAWINPUT* read(LPARAM lParam) {
        struct Buffer { alignas(RAWINPUT) BYTE data[512]; };
        static Buffer buffer;

        UINT size = sizeof(buffer.data);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
            return nullptr;
        }
        return reinterpret_cast<const RAWINPUT*>(buffer.data);
    }
}