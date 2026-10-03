#pragma once
#include <algorithm>
#include <DirectXMath.h>

// Cámara orbital en 3ª persona: gira alrededor de un objetivo (el jugador).
class Camera {
    public:
    static constexpr float DISTANCE      = 7.0f;
    static constexpr float TARGET_HEIGHT = 0.7f;    // el punto de mira queda algo elevado sobre la base del objetivo
    static constexpr float SENSITIVITY   = 0.0025f; // radianes por píxel
    static constexpr float MIN_PITCH     = 0.05f;
    static constexpr float MAX_PITCH     = 1.45f;

    Camera() { updateOffset(); }

    void rotate(float dx, float dy) {
        if (dx == 0.0f && dy == 0.0f) return;

        m_yaw = DirectX::XMScalarModAngle(m_yaw + dx * SENSITIVITY);
        // Ratón hacia arriba = cámara más baja (mirando hacia arriba).
        m_pitch = (std::clamp)(m_pitch + dy * SENSITIVITY, MIN_PITCH, MAX_PITCH);
        updateOffset();
    }

    // Ángulo horizontal hacia el que mira la cámara (0 = hacia +Z).
    float yaw() const { return m_yaw; }

    DirectX::XMMATRIX viewMatrix(const DirectX::XMFLOAT3& target) const {
        const DirectX::XMVECTOR at = DirectX::XMVectorSet(target.x, target.y + TARGET_HEIGHT, target.z, 1.0f);
        const DirectX::XMVECTOR eye = DirectX::XMVectorAdd(at, DirectX::XMLoadFloat3(&m_offset));
        return DirectX::XMMatrixLookAtLH(eye, at, DirectX::g_XMIdentityR1);
    }

    private:
    // La posición orbital solo se recalcula cuando cambian los ángulos, no en cada frame.
    void updateOffset() {
        float sinYaw, cosYaw, sinPitch, cosPitch;
        DirectX::XMScalarSinCos(&sinYaw, &cosYaw, m_yaw);
        DirectX::XMScalarSinCos(&sinPitch, &cosPitch, m_pitch);
        m_offset = { -sinYaw * cosPitch * DISTANCE, sinPitch * DISTANCE, -cosYaw * cosPitch * DISTANCE };
    }

    float m_yaw = 0.0f;
    float m_pitch = 0.6f;
    DirectX::XMFLOAT3 m_offset = {};
};