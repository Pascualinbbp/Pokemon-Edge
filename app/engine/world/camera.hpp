#pragma once
#include <algorithm>
#include <DirectXMath.h>

// Cámara orbital en 3ª persona: gira alrededor de un objetivo (el jugador).
// Al apuntar pasa suavemente a una cámara sobre el hombro derecho (como al apuntar un arma en GTA).
class Camera {
    public:
    // Cámara normal
    static constexpr float DISTANCE      = 7.0f;
    static constexpr float TARGET_HEIGHT = 0.7f;    // el punto de mira queda algo elevado sobre la base del objetivo
    static constexpr float SENSITIVITY   = 0.0025f; // radianes por píxel
    static constexpr float MIN_PITCH     = 0.05f;
    static constexpr float MAX_PITCH     = 1.45f;
    static constexpr float FOV           = 0.7853982f; // 45°

    // Cámara de apuntado
    static constexpr float AIM_DISTANCE     = 3.2f;
    static constexpr float AIM_TARGET_HEIGHT = 1.3f;   // altura del hombro
    static constexpr float AIM_SHOULDER     = 0.8f;    // desplazamiento lateral a la derecha
    static constexpr float AIM_FOV          = 0.62f;
    static constexpr float AIM_MIN_PITCH    = -0.3f;   // al apuntar se puede mirar algo hacia arriba
    static constexpr float AIM_SENSITIVITY_SCALE = 0.6f; // más precisión al apuntar
    static constexpr float BLEND_SPEED      = 7.0f;    // la transición dura ~1/7 s

    // Rotación libre alrededor del jugador (no afecta a hacia dónde camina el personaje).
    void rotate(float dx, float dy) {
        if (dx == 0.0f && dy == 0.0f) return;

        const float sensitivity = SENSITIVITY * lerp(1.0f, AIM_SENSITIVITY_SCALE, blend());
        m_yaw = DirectX::XMScalarModAngle(m_yaw + dx * sensitivity);
        // Ratón hacia arriba = cámara más baja (mirando hacia arriba).
        m_pitch = (std::clamp)(m_pitch + dy * sensitivity, minPitch(), MAX_PITCH);
    }

    // Avanza la transición entre cámara normal y de apuntado.
    void update(float dt, bool aiming) {
        const float target = aiming ? 1.0f : 0.0f;
        const float step = BLEND_SPEED * dt;
        m_aim = m_aim < target ? (std::min)(m_aim + step, target) : (std::max)(m_aim - step, target);
        m_pitch = (std::max)(m_pitch, minPitch());
    }

    // Ángulo horizontal hacia el que mira la cámara (0 = hacia +Z).
    float yaw() const { return m_yaw; }

    // 0 = cámara normal, 1 = apuntando (con suavizado).
    float aimBlend() const { return blend(); }

    float fov() const { return lerp(FOV, AIM_FOV, blend()); }

    // Dirección hacia la que mira la cámara (unitaria).
    DirectX::XMFLOAT3 forward() const {
        float sinYaw, cosYaw, sinPitch, cosPitch;
        angles(sinYaw, cosYaw, sinPitch, cosPitch);
        return { sinYaw * cosPitch, -sinPitch, cosYaw * cosPitch };
    }

    // Posición de la cámara en el mundo.
    DirectX::XMFLOAT3 eye(const DirectX::XMFLOAT3& target) const {
        DirectX::XMFLOAT3 at, eyePos;
        compute(target, at, eyePos);
        return eyePos;
    }

    DirectX::XMMATRIX viewMatrix(const DirectX::XMFLOAT3& target) const {
        DirectX::XMFLOAT3 at, eyePos;
        compute(target, at, eyePos);
        return DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eyePos), DirectX::XMLoadFloat3(&at), DirectX::g_XMIdentityR1);
    }

    private:
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    float blend() const { return m_aim * m_aim * (3.0f - 2.0f * m_aim); } // smoothstep
    float minPitch() const { return lerp(MIN_PITCH, AIM_MIN_PITCH, blend()); }

    void angles(float& sinYaw, float& cosYaw, float& sinPitch, float& cosPitch) const {
        DirectX::XMScalarSinCos(&sinYaw, &cosYaw, m_yaw);
        DirectX::XMScalarSinCos(&sinPitch, &cosPitch, m_pitch);
    }

    // Punto al que mira la cámara y posición de la cámara, mezclando cámara normal y de apuntado.
    void compute(const DirectX::XMFLOAT3& target, DirectX::XMFLOAT3& at, DirectX::XMFLOAT3& eyePos) const {
        const float t = blend();
        float sinYaw, cosYaw, sinPitch, cosPitch;
        angles(sinYaw, cosYaw, sinPitch, cosPitch);

        const float distance = lerp(DISTANCE, AIM_DISTANCE, t);
        const float height = lerp(TARGET_HEIGHT, AIM_TARGET_HEIGHT, t);
        const float shoulder = AIM_SHOULDER * t;

        // Derecha de la cámara = (cosYaw, 0, -sinYaw).
        at = { target.x + cosYaw * shoulder, target.y + height, target.z - sinYaw * shoulder };
        eyePos = { at.x - sinYaw * cosPitch * distance, at.y + sinPitch * distance, at.z - cosYaw * cosPitch * distance };
    }

    float m_yaw = 0.0f;
    float m_pitch = 0.6f;
    float m_aim = 0.0f; // progreso lineal de la transición (0..1)
};
