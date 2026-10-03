#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

// Cámara orbital en 3ª persona: gira alrededor de un objetivo (el jugador).
class Camera {
    public:
    static constexpr float DISTANCE      = 7.0f;
    static constexpr float TARGET_HEIGHT = 0.7f;    // el punto de mira queda algo elevado sobre la base del objetivo
    static constexpr float SENSITIVITY   = 0.0025f; // radianes por píxel
    static constexpr float MIN_PITCH     = 0.05f;
    static constexpr float MAX_PITCH     = 1.45f;

    void rotate(float dx, float dy) {
        m_yaw += dx * SENSITIVITY;
        if (m_yaw > DirectX::XM_PI) m_yaw -= DirectX::XM_2PI;
        else if (m_yaw < -DirectX::XM_PI) m_yaw += DirectX::XM_2PI;

        // Ratón hacia arriba = cámara más baja (mirando hacia arriba).
        m_pitch = (std::clamp)(m_pitch + dy * SENSITIVITY, MIN_PITCH, MAX_PITCH);
    }

    // Ángulo horizontal hacia el que mira la cámara (0 = hacia +Z).
    float yaw() const { return m_yaw; }

    DirectX::XMMATRIX viewMatrix(const DirectX::XMFLOAT3& target) const {
        const float cosPitch = std::cos(m_pitch);
        const float ax = target.x;
        const float ay = target.y + TARGET_HEIGHT;
        const float az = target.z;

        const DirectX::XMVECTOR at  = DirectX::XMVectorSet(ax, ay, az, 1.0f);
        const DirectX::XMVECTOR eye = DirectX::XMVectorSet(
            ax - std::sin(m_yaw) * cosPitch * DISTANCE,
            ay + std::sin(m_pitch) * DISTANCE,
            az - std::cos(m_yaw) * cosPitch * DISTANCE,
            1.0f);
        const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return DirectX::XMMatrixLookAtLH(eye, at, up);
    }

    private:
    float m_yaw = 0.0f;
    float m_pitch = 0.6f;
};