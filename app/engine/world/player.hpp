#pragma once
#include <DirectXMath.h>
#include "../core/input.hpp"

class Player {
    public:
    static constexpr float WALK_SPEED   = 5.0f;
    static constexpr float SPRINT_SPEED = 9.0f;
    static constexpr float CROUCH_SPEED = 2.5f;
    static constexpr float SLIDE_SPEED  = 12.0f; // velocidad inicial del deslizamiento
    static constexpr float SLIDE_DECEL  = 16.0f; // lo que frena por segundo
    static constexpr float JUMP_SPEED   = 7.0f;
    static constexpr float GRAVITY      = 20.0f;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f }; // y = 0 es el suelo

    // Escala vertical del modelo según la postura (de pie, agachado o deslizándose).
    float heightScale() const { return m_heightScale; }

    // El movimiento es relativo a hacia dónde mira la cámara (cameraYaw).
    void update(float dt, const InputState& input, float cameraYaw) {
        float dirX = 0.0f, dirZ = 0.0f;
        const bool moving = readDirection(input, cameraYaw, dirX, dirZ);
        const bool grounded = position.y == 0.0f; // se fuerza a 0 exacto al aterrizar
        const bool forward = input.up && !input.down;

        // Correr: doble toque en W, solo desde el suelo y sin agacharse. Termina al soltar W.
        if (!forward) m_sprinting = false;
        else if (input.sprint && grounded && !input.crouch) m_sprinting = true;

        // Deslizarse: corriendo + agacharse, ya sea en el suelo o al aterrizar de un salto corriendo.
        if (m_sliding) {
            m_slideSpeed -= SLIDE_DECEL * dt;
            if (!input.crouch || m_slideSpeed <= CROUCH_SPEED) m_sliding = false;
        } else if (grounded && m_sprinting && input.crouch) {
            m_sliding = true;
            m_sprinting = false;
            m_slideSpeed = SLIDE_SPEED;
            m_slideDirX = dirX; // la dirección queda fijada durante el deslizamiento
            m_slideDirZ = dirZ;
        }

        if (m_sliding) {
            position.x += m_slideDirX * m_slideSpeed * dt;
            position.z += m_slideDirZ * m_slideSpeed * dt;
        } else if (moving) {
            const float speed = m_sprinting ? SPRINT_SPEED : (grounded && input.crouch ? CROUCH_SPEED : WALK_SPEED);
            position.x += dirX * speed * dt;
            position.z += dirZ * speed * dt;
        }

        // Salto: solo con una pulsación nueva y desde el suelo.
        if (grounded && input.jump) {
            m_velocityY = JUMP_SPEED;
            m_sliding = false;
        }

        if (!grounded || m_velocityY > 0.0f) {
            m_velocityY -= GRAVITY * dt;
            position.y += m_velocityY * dt;
            if (position.y <= 0.0f) {
                position.y = 0.0f;
                m_velocityY = 0.0f;
            }
        }

        m_heightScale = m_sliding ? SLIDE_HEIGHT : (input.crouch ? CROUCH_HEIGHT : 1.0f);
    }

    private:
    static constexpr float INV_SQRT2 = 0.70710678f;
    static constexpr float CROUCH_HEIGHT = 0.6f;
    static constexpr float SLIDE_HEIGHT = 0.45f;

    // Dirección de movimiento (vector unitario en XZ) a partir de WASD y la orientación de la cámara.
    static bool readDirection(const InputState& input, float cameraYaw, float& x, float& z) {
        const float forward = static_cast<float>(input.up) - static_cast<float>(input.down);
        const float strafe = static_cast<float>(input.right) - static_cast<float>(input.left);
        if (forward == 0.0f && strafe == 0.0f) return false;

        float s, c;
        DirectX::XMScalarSinCos(&s, &c, cameraYaw);

        // Adelante = (s, c), derecha = (c, -s). En diagonal se escala para no ir más rápido.
        const float scale = (forward != 0.0f && strafe != 0.0f) ? INV_SQRT2 : 1.0f;
        x = (forward * s + strafe * c) * scale;
        z = (forward * c - strafe * s) * scale;
        return true;
    }

    float m_velocityY = 0.0f;
    float m_slideSpeed = 0.0f;
    float m_slideDirX = 0.0f;
    float m_slideDirZ = 0.0f;
    float m_heightScale = 1.0f;
    bool m_sprinting = false;
    bool m_sliding = false;
};