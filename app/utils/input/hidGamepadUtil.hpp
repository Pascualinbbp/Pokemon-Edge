#pragma once
#include <cstdint>
#include <vector>
#include <windows.h>
#include "gamepadState.hpp"
#include "rawInputUtil.hpp"

// Mandos de PlayStation (DualShock 4 y DualSense) leídos directamente desde Raw Input (HID).
// Solo se interpretan los informes conocidos: USB y Bluetooth (modo básico y completo).
namespace HidGamepadUtil {
    namespace detail {
        inline constexpr USHORT SONY_VENDOR_ID = 0x054C;

        enum class Model { NONE, DUALSHOCK4, DUALSENSE };

        inline HANDLE device = nullptr;
        inline Model model = Model::NONE;
        inline GamepadState state;

        inline Model modelFromProduct(USHORT productId) {
            switch (productId) {
                case 0x05C4: case 0x09CC: case 0x0BA0: return Model::DUALSHOCK4;
                case 0x0CE6: case 0x0DF2:               return Model::DUALSENSE;
                default:                                return Model::NONE;
            }
        }

        inline float stick(BYTE value) { return (static_cast<float>(value) - 127.5f) / 127.5f; }

        inline void addButton(GamepadState& s, bool pressed, uint16_t button) {
            if (pressed) s.buttons = static_cast<uint16_t>(s.buttons | button);
        }

        inline void reset() {
            device = nullptr;
            model = Model::NONE;
            state = {};
        }

        // Interpreta un informe de entrada. 'size' incluye el byte del identificador.
        inline void parse(const BYTE* report, DWORD size) {
            const BYTE* axes = nullptr;       // primer byte de los ejes (stick izquierdo X)
            bool dualSenseLayout = false;     // el DualSense por USB coloca gatillos y botones de otra forma

            if (model == Model::DUALSHOCK4) {
                if (report[0] == 0x01) axes = report + 1;                          // USB / Bluetooth básico
                else if (report[0] == 0x11 && size >= 12) axes = report + 3;       // Bluetooth completo
            } else {
                if (report[0] == 0x01) {                                           // USB (64 bytes) / Bluetooth básico (10)
                    axes = report + 1;
                    dualSenseLayout = size > 10;
                } else if (report[0] == 0x31 && size >= 12) {                      // Bluetooth completo
                    axes = report + 2;
                    dualSenseLayout = true;
                }
            }
            if (!axes) return;

            const BYTE* buttons = axes + (dualSenseLayout ? 7 : 4); // [0] hat + frontales, [1] hombros y sistema
            GamepadState s;
            s.lx = stick(axes[0]);
            s.ly = -stick(axes[1]);
            s.rx = stick(axes[2]);
            s.ry = -stick(axes[3]);
            s.lt = static_cast<float>(dualSenseLayout ? axes[4] : axes[7]) / 255.0f;
            s.rt = static_cast<float>(dualSenseLayout ? axes[5] : axes[8]) / 255.0f;

            addButton(s, buttons[0] & 0x20, Gamepad::SOUTH); // ✕
            addButton(s, buttons[0] & 0x40, Gamepad::EAST);  // ○
            addButton(s, buttons[0] & 0x10, Gamepad::WEST);  // □
            addButton(s, buttons[0] & 0x80, Gamepad::NORTH); // △
            addButton(s, buttons[1] & 0x01, Gamepad::L1);
            addButton(s, buttons[1] & 0x02, Gamepad::R1);
            addButton(s, buttons[1] & 0x10, Gamepad::BACK);  // Share / Create
            addButton(s, buttons[1] & 0x20, Gamepad::START); // Options
            addButton(s, buttons[1] & 0x40, Gamepad::L3);
            addButton(s, buttons[1] & 0x80, Gamepad::R3);

            const int hat = buttons[0] & 0x0F; // 0 = arriba, sentido horario; 8 = sin pulsar
            addButton(s, hat == 7 || hat == 0 || hat == 1, Gamepad::DPAD_UP);
            addButton(s, hat >= 1 && hat <= 3, Gamepad::DPAD_RIGHT);
            addButton(s, hat >= 3 && hat <= 5, Gamepad::DPAD_DOWN);
            addButton(s, hat >= 5 && hat <= 7, Gamepad::DPAD_LEFT);

            state = s;
        }
    }

    inline bool connected() { return detail::device != nullptr; }
    inline const GamepadState& state() { return detail::state; }

    // Busca un mando de Sony entre los dispositivos HID. Solo se llama cuando cambian los dispositivos.
    inline bool rescan() {
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0) {
            detail::reset();
            return false;
        }
        std::vector<RAWINPUTDEVICELIST> list(count);
        if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) {
            detail::reset();
            return false;
        }

        for (UINT i = 0; i < count; ++i) {
            if (list[i].dwType != RIM_TYPEHID) continue;

            RID_DEVICE_INFO info = {};
            info.cbSize = sizeof(info);
            UINT size = sizeof(info);
            if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1)) continue;
            if (info.hid.dwVendorId != detail::SONY_VENDOR_ID ||
                info.hid.usUsagePage != RawInputUtil::USAGE_PAGE_GENERIC_DESKTOP ||
                info.hid.usUsage != RawInputUtil::USAGE_GAMEPAD) continue;

            const detail::Model model = detail::modelFromProduct(static_cast<USHORT>(info.hid.dwProductId));
            if (model == detail::Model::NONE) continue;

            if (detail::device != list[i].hDevice) {
                detail::device = list[i].hDevice;
                detail::model = model;
                detail::state = {};
            }
            return true;
        }

        detail::reset();
        return false;
    }

    // Informe HID recibido por WM_INPUT. Solo se procesan los del mando seleccionado.
    inline void onReport(const RAWINPUT& raw) {
        if (raw.header.hDevice != detail::device) return;

        const RAWHID& hid = raw.data.hid;
        if (hid.dwCount == 0 || hid.dwSizeHid < 10) return;
        detail::parse(hid.bRawData + (hid.dwCount - 1) * hid.dwSizeHid, hid.dwSizeHid); // el más reciente
    }
}