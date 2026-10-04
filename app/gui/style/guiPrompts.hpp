#pragma once
#include "imgui.h"
#include "guiDraw.hpp"
#include "../../engine/core/inputDevice.hpp"

// Ayudas de "qué botón pulsar" dentro del juego. El icono depende del dispositivo en uso, así que
// cambian solas al pasar de teclado a mando (y de un mando a otro).
namespace GuiPrompts {
    enum class Action { JUMP, CROUCH, SPRINT, PAUSE, AIM, THROW, LOCK };

    namespace detail {
        inline constexpr float HEIGHT = 24.0f;
        inline constexpr float RADIUS = 12.0f;

        // Texto de la tecla o botón cuando no se dibuja como botón frontal.
        inline const char* keyText(Action action, InputDevice device) {
            if (!isGamepad(device)) {
                switch (action) {
                    case Action::JUMP:   return "ESPACIO";
                    case Action::CROUCH: return "SHIFT";
                    case Action::SPRINT: return "W W";
                    case Action::PAUSE:  return "ESC";
                    case Action::AIM:    return "CLIC DER.";
                    case Action::THROW:  return "CLIC IZQ.";
                    case Action::LOCK:   return "TAB";
                }
            }
            switch (action) {
                case Action::SPRINT: return "L3";
                case Action::PAUSE:  return device == InputDevice::XBOX ? "MENU" : "OPTIONS";
                case Action::AIM:    return "L2";
                case Action::THROW:  return "R2";
                case Action::LOCK:   return "R3";
                default:             return "";
            }
        }
    }

    // Dibuja el icono de 'action' para 'device' seguido de 'text'. Devuelve el ancho ocupado.
    inline float draw(ImDrawList* dl, const ImVec2& pos, Action action, const char* text, InputDevice device) {
        using namespace GuiDraw;

        float iconWidth;
        const bool faceAction = action == Action::JUMP || action == Action::CROUCH;
        if (isGamepad(device) && faceAction) {
            faceButton(dl, ImVec2(pos.x + detail::RADIUS, pos.y + detail::HEIGHT * 0.5f),
                action == Action::JUMP ? Face::SOUTH : Face::EAST, device == InputDevice::XBOX, true, detail::RADIUS);
            iconWidth = detail::RADIUS * 2.0f;
        } else {
            iconWidth = keycap(dl, pos, detail::keyText(action, device), detail::HEIGHT, detail::HEIGHT);
        }

        label(dl, pos.x + iconWidth + 8.0f, pos.y + detail::HEIGHT * 0.5f, text);
        return iconWidth + 8.0f + ImGui::CalcTextSize(text).x;
    }
}
