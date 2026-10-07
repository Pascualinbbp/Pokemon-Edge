#pragma once
#include <cmath>
#include <optional>
#include <windows.h>
#include "../../engine/core/input.hpp"
#include "../../engine/core/inputBindings.hpp"
#include "../../engine/core/inputDevice.hpp"
#include "../../utils/input/cursorUtil.hpp"
#include "../../utils/input/hidGamepadUtil.hpp"
#include "../../utils/input/rawInputUtil.hpp"
#include "../../utils/input/xinputUtil.hpp"

// Entrada de la plataforma (Win32): teclado, ratón y mandos.
// Convierte cualquier dispositivo en un InputState y decide cuál es el dispositivo activo.
namespace InputHandler {
    namespace detail {
        inline constexpr DWORD DOUBLE_TAP_MS = 300;
        inline constexpr LPARAM REPEAT_FLAG = 1 << 30;      // bit 30 de lParam: la tecla ya estaba pulsada
        inline constexpr UINT LEFT_SHIFT_SCANCODE = 0x2A;

        // --- Teclado y ratón ---
        struct Keyboard {
            bool up = false, down = false, left = false, right = false; // mantenidas
            bool jump = false, crouch = false, sprint = false;          // eventos de un frame
            bool interact = false;                                      // F
            bool inventory = false;                                     // I (solo mientras se juega)
            bool throwBall = false;                                     // clic izquierdo
            bool aimToggle = false;                                     // clic derecho
            bool escape = false;                                        // ESC (solo mientras se juega)
            bool lockTap = false;                                       // TAB pulsado y soltado rápido
            bool tabDown = false, tabFired = false;                     // TAB mantenido
            int ballSwitch = 0;                                         // Q (-1), E (+1) o rueda del ratón
            int teamSelect = 0;                                         // teclas 1..6 (solo mientras se juega)
            DWORD tabSince = 0;
        };
        inline Keyboard keys;
        inline float mouseDX = 0.0f;
        inline float mouseDY = 0.0f;
        inline bool captured = false;
        inline DWORD lastWPress = 0;
        inline int lastMouseX = -1;
        inline int lastMouseY = -1;
        inline HWND window = nullptr;

        // --- Dispositivos ---
        inline InputDevice active = InputDevice::KEYBOARD_MOUSE;
        inline std::optional<InputDevice> padType;  // tipo del mando conectado; vacío si no hay
        inline uint16_t padPrevButtons = 0;
        inline bool padPrevTriggerDown = false;     // R2 en el frame anterior (para detectar la pulsación)
        inline DWORD padLockSince = 0;              // R3 pulsado desde...
        inline bool padLockFired = false;
        inline bool sonyRegistered = false;
        inline bool devicesDirty = false;

        // Descarta lo que no debe arrastrarse entre frames o entre estados (pausa, carga...).
        inline void clearEvents() {
            mouseDX = mouseDY = 0.0f;
            keys.jump = keys.crouch = keys.sprint = keys.throwBall = keys.interact = keys.inventory = false;
            keys.aimToggle = keys.escape = keys.lockTap = false;
            keys.ballSwitch = 0;
            keys.teamSelect = 0;
        }

        inline bool readPad(GamepadState& out) {
            if (!padType) return false;
            if (*padType == InputDevice::XBOX) return XInputUtil::read(out);
            out = HidGamepadUtil::state();
            return HidGamepadUtil::connected();
        }

        // Al conectar un mando se pasa a él; al desconectarlo, de vuelta a teclado y ratón.
        // Los informes HID de PlayStation solo se piden a Windows mientras haya un mando de ese tipo.
        inline void setPad(std::optional<InputDevice> type) {
            const bool sony = type == InputDevice::PLAYSTATION;
            if (sony != sonyRegistered) {
                sonyRegistered = sony;
                RawInputUtil::registerDevice(window, RawInputUtil::USAGE_GAMEPAD, sony);
            }

            if (type == padType) return;
            padType = type;
            active = type.value_or(InputDevice::KEYBOARD_MOUSE);
            padPrevButtons = 0;
            padPrevTriggerDown = false;
        }

        inline void rescanGamepads() {
            if (XInputUtil::rescan()) setPad(InputDevice::XBOX);
            else if (HidGamepadUtil::rescan()) setPad(InputDevice::PLAYSTATION);
            else setPad(std::nullopt);
        }

        // Evita que un botón ya mantenido al empezar a jugar cuente como pulsación nueva.
        inline void primePad() {
            GamepadState pad;
            const bool ok = readPad(pad);
            padPrevButtons = ok ? pad.buttons : 0;
            padPrevTriggerDown = ok && pad.rt > InputBindings::TRIGGER_THRESHOLD;
        }
    }

    inline void init(HWND hwnd) {
        detail::window = hwnd;
        detail::rescanGamepads();
    }

    // Windows avisa de cambios de dispositivos (WM_DEVICECHANGE): solo se marca; la búsqueda se hace
    // una vez por iteración del bucle principal.
    inline void onDeviceChange() { detail::devicesDirty = true; }

    inline void refreshDevices() {
        if (!detail::devicesDirty) return;
        detail::devicesDirty = false;
        detail::rescanGamepads();
    }

    inline InputDevice activeDevice() { return detail::active; }
    inline bool hasGamepad() { return detail::padType.has_value(); }

    inline void onKey(WPARAM key, LPARAM lParam, bool pressed) {
        detail::Keyboard& k = detail::keys;
        const bool newPress = pressed && !(lParam & detail::REPEAT_FLAG); // ignora la repetición automática
        bool gameKey = true;

        switch (key) {
            case 'W':
                if (newPress) {
                    const DWORD now = GetMessageTime();
                    if (now - detail::lastWPress <= detail::DOUBLE_TAP_MS) {
                        k.sprint = true;
                        detail::lastWPress = 0; // un tercer toque no cuenta como otro doble toque
                    } else {
                        detail::lastWPress = now;
                    }
                }
                k.up = pressed;
                break;
            case 'S': k.down = pressed; break;
            case 'A': k.left = pressed; break;
            case 'D': k.right = pressed; break;
            case 'Q':
                if (newPress) k.ballSwitch = -1;
                break;
            case 'E':
                if (newPress) k.ballSwitch = 1;
                break;
            case 'F':
                if (newPress) k.interact = true;
                break;
            case 'I':
                if (newPress && detail::captured) k.inventory = true;
                break;
            case '1': case '2': case '3': case '4': case '5': case '6':
                if (newPress && detail::captured) k.teamSelect = static_cast<int>(key - '0');
                break;
            case VK_SPACE:
                if (newPress) k.jump = true;
                break;
            case VK_SHIFT: // solo el shift izquierdo (por su scancode)
                if (newPress && ((lParam >> 16) & 0xFF) == detail::LEFT_SHIFT_SCANCODE) k.crouch = true;
                break;
            case VK_TAB:
                if (newPress && detail::captured) {
                    k.tabDown = true;
                    k.tabFired = false;
                    k.tabSince = GetTickCount();
                } else if (!pressed) {
                    if (k.tabDown && !k.tabFired) k.lockTap = true;
                    k.tabDown = false;
                }
                break;
            case VK_ESCAPE:
                if (newPress && detail::captured) k.escape = true;
                gameKey = false;
                break;
            default:
                gameKey = false;
                break;
        }

        if (gameKey && newPress) detail::active = InputDevice::KEYBOARD_MOUSE;
    }

    // Botones del ratón: clic derecho = entrar / salir del modo lanzamiento, clic izquierdo = lanzar.
    // Solo cuentan las pulsaciones mientras se juega.
    inline void onMouseButton(bool right, bool pressed) {
        detail::Keyboard& k = detail::keys;
        if (!pressed || !detail::captured) return;

        if (right) k.aimToggle = true;
        else k.throwBall = true;
        detail::active = InputDevice::KEYBOARD_MOUSE;
    }

    // Rueda del ratón: cambia de pokéball (hacia delante = anterior, hacia atrás = siguiente).
    inline void onMouseWheel(int delta) {
        if (!detail::captured || delta == 0) return;
        detail::keys.ballSwitch = delta > 0 ? -1 : 1;
        detail::active = InputDevice::KEYBOARD_MOUSE;
    }

    // Movimiento del ratón sobre la ventana (menús). Mover el ratón pasa la interfaz a teclado y ratón.
    inline void onMouseMove(LPARAM lParam) {
        if (detail::captured) return; // jugando, el ratón llega por Raw Input
        const int x = static_cast<short>(LOWORD(lParam));
        const int y = static_cast<short>(HIWORD(lParam));
        if (x == detail::lastMouseX && y == detail::lastMouseY) return;

        detail::lastMouseX = x;
        detail::lastMouseY = y;
        detail::active = InputDevice::KEYBOARD_MOUSE;
    }

    // WM_INPUT: movimiento del ratón o informe HID de un mando de PlayStation.
    inline void onRawInput(LPARAM lParam) {
        const RAWINPUT* raw = RawInputUtil::read(lParam);
        if (!raw) return;

        if (raw->header.dwType == RIM_TYPEMOUSE) {
            if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) return;
            const LONG dx = raw->data.mouse.lLastX;
            const LONG dy = raw->data.mouse.lLastY;
            if (dx == 0 && dy == 0) return;

            detail::mouseDX += static_cast<float>(dx);
            detail::mouseDY += static_cast<float>(dy);
            detail::active = InputDevice::KEYBOARD_MOUSE;
        } else if (raw->header.dwType == RIM_TYPEHID) {
            HidGamepadUtil::onReport(*raw);
        }
    }

    // Evita teclas "pegadas" al perder el foco.
    inline void resetKeys() { detail::keys = detail::Keyboard{}; }

    // Si la ventana se mueve o cambia de tamaño durante la captura, recoloca el bloqueo.
    inline void relockCursor(HWND hwnd) {
        if (detail::captured) CursorUtil::lockToCenter(hwnd);
    }

    // Oculta y bloquea el cursor y activa el Raw Input del ratón solo mientras se juega. Es idempotente.
    inline void setMouseCapture(HWND hwnd, bool capture) {
        if (capture == detail::captured) return;
        detail::captured = capture;
        detail::clearEvents();
        detail::keys.tabDown = false;
        detail::padLockFired = false;
        detail::padLockSince = 0;
        RawInputUtil::registerDevice(detail::window, RawInputUtil::USAGE_MOUSE, capture);

        if (capture) {
            detail::primePad();
            CursorUtil::capture(hwnd);
        } else {
            CursorUtil::release();
        }
    }

    // Lee el mando conectado con las zonas muertas aplicadas. Si el mando muestra actividad, pasa a ser
    // el dispositivo activo. Si se ha perdido, vuelve a buscar. Devuelve false si no hay mando.
    inline bool readGamepad(GamepadState& pad) {
        if (!detail::padType) return false;
        if (!detail::readPad(pad)) {
            detail::rescanGamepads();
            return false;
        }

        applyDeadzone(pad.lx, pad.ly, InputBindings::STICK_DEADZONE);
        applyDeadzone(pad.rx, pad.ry, InputBindings::STICK_DEADZONE);
        if (pad.hasActivity()) detail::active = *detail::padType;
        return true;
    }

    // Estado de entrada de este frame del dispositivo activo; consume los eventos acumulados.
    inline InputState poll(float dt) {
        GamepadState pad;
        const bool padOk = readGamepad(pad);
        const bool triggerDown = padOk && pad.rt > InputBindings::TRIGGER_THRESHOLD;

        InputState input;
        if (padOk && isGamepad(detail::active)) {
            const uint16_t pressed = static_cast<uint16_t>(pad.buttons & ~detail::padPrevButtons);
            input.moveX = pad.lx;
            input.moveY = pad.ly;
            // Respuesta cuadrática: más precisión con inclinaciones pequeñas. Arriba en el stick = mirar arriba.
            input.lookX = pad.rx * std::fabs(pad.rx) * InputBindings::LOOK_SPEED * dt;
            input.lookY = -pad.ry * std::fabs(pad.ry) * InputBindings::LOOK_SPEED * dt;
            input.jump = (pressed & InputBindings::PAD_JUMP) != 0;
            input.crouch = (pressed & InputBindings::PAD_CROUCH) != 0;
            input.sprint = (pressed & InputBindings::PAD_SPRINT) != 0;
            input.pause = (pressed & InputBindings::PAD_PAUSE) != 0;
            input.interact = (pressed & InputBindings::PAD_INTERACT) != 0;
            input.inventory = (pressed & InputBindings::PAD_INVENTORY) != 0;
            input.aimHold = pad.lt > InputBindings::TRIGGER_THRESHOLD;
            input.throwBall = triggerDown && !detail::padPrevTriggerDown;
            if (pressed & InputBindings::PAD_BALL_PREV) input.ballSwitch = -1;
            else if (pressed & InputBindings::PAD_BALL_NEXT) input.ballSwitch = 1;

            // R3: pulsación corta = fijar / cambiar; mantenido = soltar.
            const bool lockDown = (pad.buttons & InputBindings::PAD_LOCK) != 0;
            const DWORD now = GetTickCount();
            if (pressed & InputBindings::PAD_LOCK) {
                detail::padLockSince = now;
                detail::padLockFired = false;
            }
            if (lockDown) {
                if (!detail::padLockFired && now - detail::padLockSince >= InputBindings::LOCK_HOLD_MS) {
                    detail::padLockFired = true;
                    input.lockCancel = true;
                }
            } else if (detail::padPrevButtons & InputBindings::PAD_LOCK) {
                if (!detail::padLockFired) input.lockTap = true;
                detail::padLockFired = false;
            }
        } else {
            const detail::Keyboard& k = detail::keys;
            input.moveX = static_cast<float>(k.right) - static_cast<float>(k.left);
            input.moveY = static_cast<float>(k.up) - static_cast<float>(k.down);
            input.lookX = detail::mouseDX;
            input.lookY = detail::mouseDY;
            input.jump = k.jump;
            input.crouch = k.crouch;
            input.sprint = k.sprint;
            input.interact = k.interact;
            input.inventory = k.inventory;
            input.throwBall = k.throwBall;
            input.aimToggle = k.aimToggle;
            input.escape = k.escape;
            input.lockTap = k.lockTap;
            input.ballSwitch = k.ballSwitch;
            input.teamSelect = k.teamSelect;
            if (k.tabDown && !k.tabFired && GetTickCount() - k.tabSince >= InputBindings::LOCK_HOLD_MS) {
                detail::keys.tabFired = true;
                input.lockCancel = true;
            }
        }

        if (padOk) {
            detail::padPrevButtons = pad.buttons;
            detail::padPrevTriggerDown = triggerDown;
        }
        detail::clearEvents();
        return input;
    }
}
