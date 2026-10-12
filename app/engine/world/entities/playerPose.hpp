#pragma once
#include <cmath>
#include <algorithm>
#include "playerAction.hpp"

// Postura del personaje en este instante: ángulos de brazos, piernas e inclinación del cuerpo, y qué sostiene.
// Es pura matemática (sin Direct3D): el render la dibuja y se puede probar sin ventana.
// Convención: ángulo 0 = el miembro cuelga; positivo = se levanta hacia delante (hacia donde mira el personaje).
struct PlayerPose {
    float rightArm = 0.0f, leftArm = 0.0f;   // elevación del brazo (rad)
    float rightRoll = 0.0f, leftRoll = 0.0f; // separación lateral del brazo (rad)
    float rightLeg = 0.0f, leftLeg = 0.0f;   // balanceo de las piernas (rad)
    float lean = 0.0f;                       // inclinación del cuerpo hacia delante (rad)
    float toolTilt = 0.0f;                   // inclinación extra de lo que sostiene (rad)
    PlayerAction holding = PlayerAction::NONE; // objeto en la mano (hacha, pico, regadera, caña...)
    bool bothHands = false;                  // el objeto se sostiene con las dos manos

    struct Input {
        PlayerAction action = PlayerAction::NONE;
        float progress = 0.0f;   // avance de la acción (0..1)
        bool climbing = false;
        bool swimming = false;
        bool diving = false;
        float phase = 0.0f;      // fase del ciclo de andar / trepar / nadar (rad)
        float speed = 0.0f;      // 0..1: cuánto se mueve
        bool crouched = false;
    };

    static float ease(float t) { return t * t * (3.0f - 2.0f * t); }

    static PlayerPose make(const Input& in) {
        PlayerPose pose;
        const float swing = std::sin(in.phase) * in.speed;
        if (in.climbing) { // trepa: manos arriba alternando, piernas flexionándose
            pose.rightArm = 2.55f + 0.45f * std::sin(in.phase);
            pose.leftArm = 2.55f - 0.45f * std::sin(in.phase);
            pose.rightLeg = 0.9f * std::sin(in.phase + 1.57f);
            pose.leftLeg = -0.9f * std::sin(in.phase + 1.57f);
            pose.lean = 0.28f;
            return pose.withAction(in);
        }
        if (in.diving) { // buceando: cuerpo horizontal, brazadas largas
            pose.lean = 1.2f;
            pose.rightArm = 2.9f * std::sin(in.phase * 0.5f) + 1.2f;
            pose.leftArm = -2.9f * std::sin(in.phase * 0.5f) + 1.2f;
            pose.rightLeg = 0.5f * std::sin(in.phase * 1.5f);
            pose.leftLeg = -0.5f * std::sin(in.phase * 1.5f);
            return pose;
        }
        if (in.swimming) { // nada en superficie: brazadas por encima de la cabeza
            pose.lean = 0.55f;
            pose.rightArm = 1.6f + 1.4f * std::sin(in.phase * 0.6f);
            pose.leftArm = 1.6f - 1.4f * std::sin(in.phase * 0.6f);
            pose.rightLeg = 0.35f * std::sin(in.phase * 1.8f);
            pose.leftLeg = -0.35f * std::sin(in.phase * 1.8f);
            return pose.withAction(in);
        }
        pose.rightArm = -0.8f * swing;
        pose.leftArm = 0.8f * swing;
        pose.rightLeg = 0.7f * swing;
        pose.leftLeg = -0.7f * swing;
        return pose.withAction(in);
    }

    private:
    PlayerPose withAction(const Input& in) const {
        PlayerPose pose = *this;
        if (in.action == PlayerAction::NONE) return pose;
        const float u = (std::clamp)(in.progress, 0.0f, 1.0f);
        pose.holding = in.action;
        switch (in.action) {
            case PlayerAction::CHOP: { // el hacha sube por encima de la cabeza y cae de golpe
                const float up = u < 0.55f ? ease(u / 0.55f) : 1.0f - ease((u - 0.55f) / 0.45f);
                pose.rightArm = 0.5f + 2.2f * up;
                pose.leftArm = 0.3f + 1.9f * up;
                pose.rightRoll = 0.1f;
                pose.lean = 0.12f + 0.3f * (1.0f - up);
                pose.toolTilt = -0.9f * up;
                pose.bothHands = true;
                break;
            }
            case PlayerAction::MINE: { // el pico se levanta y se clava
                const float up = u < 0.5f ? ease(u / 0.5f) : 1.0f - ease((u - 0.5f) / 0.5f);
                pose.rightArm = 0.7f + 2.0f * up;
                pose.leftArm = 0.6f + 1.8f * up;
                pose.lean = 0.1f + 0.35f * (1.0f - up);
                pose.toolTilt = -0.6f * up;
                pose.bothHands = true;
                break;
            }
            case PlayerAction::WATER: { // la regadera se inclina hacia delante
                const float tip = std::sin(3.14159265f * u);
                pose.rightArm = 1.25f;
                pose.leftArm = 0.2f;
                pose.rightRoll = 0.25f;
                pose.toolTilt = 0.15f + 0.85f * tip;
                pose.lean = 0.08f;
                break;
            }
            case PlayerAction::PICK: { // se agacha a coger
                const float down = std::sin(3.14159265f * u);
                pose.lean = 0.2f + 0.8f * down;
                pose.rightArm = 0.5f + 0.9f * down;
                pose.leftArm = 0.3f + 0.4f * down;
                pose.rightLeg = 0.35f * down;
                pose.leftLeg = -0.2f * down;
                pose.holding = PlayerAction::NONE;
                break;
            }
            case PlayerAction::FISH: { // lanzamiento de caña: atrás y adelante, luego la sostiene
                const float cast = u < 0.4f ? -ease(u / 0.4f) : -1.0f + ease((u - 0.4f) / 0.6f) * 1.0f;
                pose.rightArm = 1.2f + 0.9f * cast;
                pose.leftArm = 0.9f + 0.5f * cast;
                pose.toolTilt = 0.5f * cast;
                pose.lean = -0.05f;
                pose.bothHands = true;
                break;
            }
            case PlayerAction::THROW: { // lanzamiento: brazo atrás y adelante
                const float back = u < 0.35f ? ease(u / 0.35f) : 1.0f - ease((u - 0.35f) / 0.65f) * 1.0f;
                pose.rightArm = 1.1f - 1.9f * back + (u > 0.35f ? 1.7f * ease((u - 0.35f) / 0.3f) : 0.0f);
                pose.lean = 0.1f - 0.1f * back;
                pose.holding = PlayerAction::NONE;
                break;
            }
            case PlayerAction::OPEN: {
                const float down = std::sin(3.14159265f * u);
                pose.lean = 0.15f + 0.5f * down;
                pose.rightArm = 0.5f + 0.8f * down;
                pose.leftArm = 0.5f + 0.8f * down;
                pose.holding = PlayerAction::NONE;
                break;
            }
            default: break;
        }
        return pose;
    }
};
