#pragma once
#include <DirectXMath.h>
#include "../core/input.hpp"

class Player {
    public:
    static constexpr float SPEED = 5.0f;
    static constexpr float JUMP_SPEED = 7.0f;
    static constexpr float GRAVITY = 20.0f;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f }; // y = 0 es el suelo

    // El movimiento es relativo a hacia dónde mira la cámara (cameraYaw).
    void update(float dt, const InputState& input, float cameraYaw) {
        move(dt, input, cameraYaw);
        jump(dt, input);
    }

    private:
    static constexpr float INV_SQRT2 = 0.70710678f;

    void move(float dt, const InputState& input, float cameraYaw) {
        const float forward = static_cast<float>(input.up) - static_cast<float>(input.down);
        const float strafe = static_cast<float>(input.right) - static_cast<float>(input.left);
        if (forward == 0.0f && strafe == 0.0f) return;

        float s, c;
        DirectX::XMScalarSinCos(&s, &c, cameraYaw);

        // Adelante = (s, c), derecha = (c, -s). En diagonal se escala para no ir más rápido.
        const float step = SPEED * dt * ((forward != 0.0f && strafe != 0.0f) ? INV_SQRT2 : 1.0f);
        position.x += (forward * s + strafe * c) * step;
        position.z += (forward * c - strafe * s) * step;
    }

    void jump(float dt, const InputState& input) {
        // En el suelo y_ == 0 exacto (se fuerza al aterrizar); sin salto no hay nada que integrar.
        if (position.y == 0.0f) {
            if (!input.jump) return;
            m_velocityY = JUMP_SPEED;
        }

        m_velocityY -= GRAVITY * dt;
        position.y += m_velocityY * dt;
        if (position.y <= 0.0f) {
            position.y = 0.0f;
            m_velocityY = 0.0f;
        }
    }

    float m_velocityY = 0.0f;
};