#pragma once
#include "imgui.h"
#include "guiDraw.hpp"
#include "../../engine/core/inputDevice.hpp"

// Ayudas de "qué botón pulsar" dentro del juego. El icono depende del dispositivo en uso, así que
// cambian solas al pasar de teclado a mando (y de un mando a otro).
namespace GuiPrompts {
    enum class Action { JUMP, CROUCH, SPRINT, PAUSE, AIM, THROW, LOCK, BALL_SWITCH, INTERACT, INVENTORY, MODE };

    namespace detail {
        inline constexpr float HEIGHT = 24.0f;
        inline constexpr float RADIUS = 12.0f;

        // Acciones que en el mando son un botón frontal (se dibujan como tal).
        inline bool faceButtonOf(Action action, GuiDraw::Face& face) {
            switch (action) {
                case Action::JUMP:     face = GuiDraw::Face::SOUTH; return true;
                case Action::CROUCH:   face = GuiDraw::Face::EAST;  return true;
                case Action::INTERACT: face = GuiDraw::Face::WEST;  return true;
                case Action::MODE:     face = GuiDraw::Face::NORTH; return true;
                default:               return false;
            }
        }

        // Texto de la tecla o botón cuando no se dibuja como botón frontal.
        inline const char* keyText(Action action, InputDevice device) {
            if (!isGamepad(device)) {
                switch (action) {
                    case Action::JUMP:        return "ESPACIO";
                    case Action::CROUCH:      return "SHIFT";
                    case Action::SPRINT:      return "W W";
                    case Action::PAUSE:       return "ESC";
                    case Action::AIM:         return "CLIC DER.";
                    case Action::THROW:       return "CLIC IZQ.";
                    case Action::LOCK:        return "TAB";
                    case Action::BALL_SWITCH: return "Q / E";
                    case Action::INTERACT:    return "F";
                    case Action::INVENTORY:   return "I";
                    case Action::MODE:        return "R";
                }
            }
            switch (action) {
                case Action::SPRINT:      return "L3";
                case Action::PAUSE:       return device == InputDevice::XBOX ? "MENU" : "OPTIONS";
                case Action::AIM:         return "L2";
                case Action::THROW:       return "R2";
                case Action::LOCK:        return "R3";
                case Action::BALL_SWITCH: return "L1 / R1";
                case Action::INVENTORY:   return device == InputDevice::XBOX ? "VER" : "SELECT";
                default:                  return "";
            }
        }
    }

    // Dibuja el icono de 'action' para 'device' seguido de 'text'. Devuelve el ancho ocupado.
    inline float draw(ImDrawList* dl, const ImVec2& pos, Action action, const char* text, InputDevice device) {
        using namespace GuiDraw;

        float iconWidth;
        Face face;
        if (isGamepad(device) && detail::faceButtonOf(action, face)) {
            faceButton(dl, ImVec2(pos.x + detail::RADIUS, pos.y + detail::HEIGHT * 0.5f), face, device == InputDevice::XBOX, true, detail::RADIUS);
            iconWidth = detail::RADIUS * 2.0f;
        } else {
            iconWidth = keycap(dl, pos, detail::keyText(action, device), detail::HEIGHT, detail::HEIGHT);
        }

        label(dl, pos.x + iconWidth + 8.0f, pos.y + detail::HEIGHT * 0.5f, text);
        return iconWidth + 8.0f + ImGui::CalcTextSize(text).x;
    }
}
