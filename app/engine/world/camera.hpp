#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

// Cámara orbital en 3ª persona: gira alrededor de un objetivo (el jugador).
// Al apuntar pasa suavemente a una cámara sobre el hombro derecho (como al apuntar un arma en GTA).
class Camera {
    public:
    // Cámara normal
    static constexpr float DISTANCE      = 7.0f;
    static constexpr float TARGET_HEIGHT = 0.7f;    // el punto de mira queda algo elevado sobre la base del objetivo
    static constexpr float SENSITIVITY   = 0.0025f; // radianes por píxel
    static constexpr float MIN_PITCH     = -0.85f; // la cámara puede ir por debajo del jugador y mirar al cielo (el suelo la frena)
    static constexpr float MAX_PITCH     = 1.53f;
    static constexpr float FOV           = 0.7853982f; // 45°

    // Cámara de apuntado
    static constexpr float LEVEL_PITCH = 0.6f;    // inclinación de partida: se considera "mirar al frente"
    static constexpr float SWIM_MIN_PITCH = -1.3f;  // nadando la cámara puede ir por debajo y mirar hacia arriba

    static constexpr float AIM_DISTANCE     = 3.2f;
    static constexpr float AIM_TARGET_HEIGHT = 1.3f;   // altura del hombro
    static constexpr float AIM_SHOULDER     = 0.8f;    // desplazamiento lateral a la derecha
    static constexpr float AIM_FOV          = 0.62f;
    static constexpr float AIM_MIN_PITCH    = -1.1f;   // al apuntar se puede mirar bien hacia arriba
    static constexpr float AIM_SENSITIVITY_SCALE = 0.6f; // más precisión al apuntar
    static constexpr float BLEND_SPEED      = 7.0f;    // la transición dura ~1/7 s
    static constexpr float LOCK_SMOOTH      = 10.0f;   // rapidez con la que la cámara se fija al objetivo

    // Rotación libre alrededor del jugador (no afecta a hacia dónde camina el personaje).
    void rotate(float dx, float dy) {
        if (dx == 0.0f && dy == 0.0f) return;

        const float sensitivity = SENSITIVITY * lerp(1.0f, AIM_SENSITIVITY_SCALE, blend());
        m_yaw = DirectX::XMScalarModAngle(m_yaw + dx * sensitivity);
        // Ratón hacia arriba = cámara más baja (mirando hacia arriba).
        m_pitch = (std::clamp)(m_pitch + dy * sensitivity, minPitch(), MAX_PITCH);
    }

    // Fijar objetivo: gira la cámara suavemente hasta que el rayo desde su posición (con el desplazamiento
    // del hombro) pase por "point". "target" es el jugador.
    void trackToward(const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& point, float dt) {
        Camera probe = *this;
        for (int i = 0; i < 4; ++i) {
            DirectX::XMFLOAT3 at, eyePos;
            probe.compute(target, at, eyePos);
            const float dx = point.x - eyePos.x;
            const float dy = point.y - eyePos.y;
            const float dz = point.z - eyePos.z;
            const float horizontal = std::sqrt(dx * dx + dz * dz);
            if (horizontal < 1.0e-3f) return;
            probe.m_yaw = std::atan2(dx, dz);
            probe.m_pitch = (std::clamp)(std::atan2(-dy, horizontal), minPitch(), MAX_PITCH);
        }

        const float k = (std::min)(1.0f, LOCK_SMOOTH * dt);
        float dYaw = DirectX::XMScalarModAngle(probe.m_yaw - m_yaw);
        m_yaw = DirectX::XMScalarModAngle(m_yaw + dYaw * k);
        m_pitch += (probe.m_pitch - m_pitch) * k;
    }

    // Avanza la transición entre cámara normal y de apuntado.
    void update(float dt, bool aiming) {
        const float target = aiming ? 1.0f : 0.0f;
        const float step = BLEND_SPEED * dt;
        m_aim = m_aim < target ? (std::min)(m_aim + step, target) : (std::max)(m_aim - step, target);
        const float low = minPitch();
        if (m_pitch < low) m_pitch += (low - m_pitch) * (std::min)(1.0f, 8.0f * dt); // al salir del agua el límite sube poco a poco: la vista no da un tirón
    }

    // Ángulo horizontal hacia el que mira la cámara (0 = hacia +Z).
    float yaw() const { return m_yaw; }

    // Hacia dónde mira respecto a "al frente" (rad): positivo hacia abajo, negativo hacia arriba.
    float lookAngle() const { return m_pitch - LEVEL_PITCH; }
    // En el agua se permite mirar hacia arriba (para subir a la superficie).
    void setSwimming(bool swimming) { m_swimming = swimming; }

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

    // Punto al que mira la cámara.
    DirectX::XMFLOAT3 focus(const DirectX::XMFLOAT3& target) const {
        DirectX::XMFLOAT3 at, eyePos;
        compute(target, at, eyePos);
        return at;
    }

    // pull: fracción (0..1) de la distancia normal a la que queda la cámara (menos de 1 cuando una pared se interpone). Nunca baja de
    // MIN_PULL: con la cámara pegada al punto al que mira (techo bajo, pared junto al jugador) la matriz de vista quedaría indefinida.
    static constexpr float MIN_PULL = 0.12f;
    DirectX::XMMATRIX viewMatrix(const DirectX::XMFLOAT3& target, float minEyeY = -1.0e9f, float maxEyeY = 1.0e9f, float pull = 1.0f) const {
        DirectX::XMFLOAT3 at, eyePos;
        compute(target, at, eyePos);
        pull = (std::clamp)(pull, MIN_PULL, 1.0f);
        eyePos = { at.x + (eyePos.x - at.x) * pull, at.y + (eyePos.y - at.y) * pull, at.z + (eyePos.z - at.z) * pull };
        eyePos.y = (std::min)((std::max)(eyePos.y, minEyeY), maxEyeY);
        return DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eyePos), DirectX::XMLoadFloat3(&at), DirectX::g_XMIdentityR1);
    }

    private:
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    float blend() const { return m_aim * m_aim * (3.0f - 2.0f * m_aim); } // smoothstep
    float minPitch() const { return m_swimming ? SWIM_MIN_PITCH : lerp(MIN_PITCH, AIM_MIN_PITCH, blend()); }

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
    float m_pitch = LEVEL_PITCH;
    bool m_swimming = false;
    float m_aim = 0.0f; // progreso lineal de la transición (0..1)
};
