#pragma once
#include <algorithm>
#include <cmath>
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
        const bool grounded = position.y == 0.0f; // se fuerza a 0 exacto al aterrizar
        const bool forward = input.moveY > FORWARD_THRESHOLD;

        float dirX = 0.0f, dirZ = 0.0f;
        const float amount = direction(input.moveX, input.moveY, cameraYaw, dirX, dirZ);

        updateSprint(input, grounded, forward);
        updateSlide(dt, input, cameraYaw, grounded);
        move(dt, input, grounded, dirX, dirZ, amount);
        updateVertical(dt, input, grounded);

        m_heightScale = m_sliding ? SLIDE_HEIGHT : (input.crouch ? CROUCH_HEIGHT : 1.0f);
    }

    private:
    static constexpr float FORWARD_THRESHOLD = 0.3f; // inclinación mínima del stick para contar como "avanzar"
    static constexpr float STEER_EPSILON = 0.05f;    // cambio mínimo de A/D o stick para girar el deslizamiento
    static constexpr float MOMENTUM_DECAY = 2.0f;    // pérdida de impulso por segundo en el aire
    static constexpr float CROUCH_HEIGHT = 0.6f;
    static constexpr float SLIDE_HEIGHT = 0.45f;

    // Dirección unitaria en XZ para unos ejes de movimiento y la orientación de la cámara.
    // Devuelve cuánto se empuja (0..1); 0 si no hay entrada. Las diagonales quedan normalizadas.
    static float direction(float strafe, float forward, float yaw, float& x, float& z) {
        const float length = std::sqrt(strafe * strafe + forward * forward);
        if (length == 0.0f) return 0.0f;

        float s, c;
        DirectX::XMScalarSinCos(&s, &c, yaw);
        // Adelante = (s, c), derecha = (c, -s).
        x = (forward * s + strafe * c) / length;
        z = (forward * c - strafe * s) / length;
        return (std::min)(length, 1.0f);
    }

    // Correr: doble toque en W / L3, solo desde el suelo y sin agacharse. Termina al dejar de avanzar.
    void updateSprint(const InputState& input, bool grounded, bool forward) {
        if (!forward) m_sprinting = false;
        else if (input.sprint && grounded && !input.crouch) m_sprinting = true;
    }

    // Deslizarse: corriendo + agacharse (en el suelo o al aterrizar de un salto corriendo).
    // Siempre avanza; A/D (o el stick) giran la dirección. La cámara no la cambia: solo se recalcula
    // cuando el jugador cambia su dirección lateral.
    void updateSlide(float dt, const InputState& input, float cameraYaw, bool grounded) {
        if (m_sliding) {
            m_slideSpeed -= SLIDE_DECEL * dt;
            if (!input.crouch || m_slideSpeed <= CROUCH_SPEED) {
                m_sliding = false;
            } else if (std::fabs(input.moveX - m_slideStrafe) > STEER_EPSILON) {
                m_slideStrafe = input.moveX;
                direction(m_slideStrafe, 1.0f, cameraYaw, m_slideDirX, m_slideDirZ);
            }
        } else if (grounded && m_sprinting && input.crouch) {
            m_sliding = true;
            m_sprinting = false;
            m_slideSpeed = SLIDE_SPEED;
            m_slideStrafe = input.moveX;
            direction(m_slideStrafe, 1.0f, cameraYaw, m_slideDirX, m_slideDirZ);
        }
    }

    void move(float dt, const InputState& input, bool grounded, float dirX, float dirZ, float amount) {
        float speed;
        if (m_sliding) {
            dirX = m_slideDirX;
            dirZ = m_slideDirZ;
            speed = m_slideSpeed;
        } else if (grounded) {
            if (amount == 0.0f) return;
            speed = (m_sprinting ? SPRINT_SPEED : (input.crouch ? CROUCH_SPEED : WALK_SPEED)) * amount;
        } else {
            // En el aire se conserva el impulso (por ejemplo, el de un salto desde un deslizamiento).
            if (amount > 0.0f) {
                m_airDirX = dirX;
                m_airDirZ = dirZ;
                speed = (std::max)((m_sprinting ? SPRINT_SPEED : WALK_SPEED) * amount, m_momentum);
            } else {
                speed = m_momentum;
            }
            m_momentum = (std::max)(m_momentum - MOMENTUM_DECAY * dt, 0.0f);
            dirX = m_airDirX;
            dirZ = m_airDirZ;
        }

        position.x += dirX * speed * dt;
        position.z += dirZ * speed * dt;
    }

    void updateVertical(float dt, const InputState& input, bool grounded) {
        // Salto: solo con una pulsación nueva y desde el suelo.
        if (grounded && input.jump) {
            m_velocityY = JUMP_SPEED;
            if (m_sliding) { // saltar desde un deslizamiento conserva el impulso de carrera
                m_sliding = false;
                m_sprinting = true;
                m_momentum = (std::max)(m_slideSpeed, SPRINT_SPEED);
                m_airDirX = m_slideDirX;
                m_airDirZ = m_slideDirZ;
            }
        }

        if (!grounded || m_velocityY > 0.0f) {
            m_velocityY -= GRAVITY * dt;
            position.y += m_velocityY * dt;
            if (position.y <= 0.0f) {
                position.y = 0.0f;
                m_velocityY = 0.0f;
                m_momentum = 0.0f;
            }
        }
    }

    float m_velocityY = 0.0f;
    float m_slideSpeed = 0.0f;
    float m_slideStrafe = 0.0f;
    float m_slideDirX = 0.0f;
    float m_slideDirZ = 0.0f;
    float m_airDirX = 0.0f;
    float m_airDirZ = 0.0f;
    float m_momentum = 0.0f;
    float m_heightScale = 1.0f;
    bool m_sprinting = false;
    bool m_sliding = false;
};