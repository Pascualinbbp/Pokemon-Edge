#pragma once
#include <DirectXMath.h>
#include "../core/input.hpp"

struct Player {
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f }; // Ajustado a nivel del suelo (y = 0)
    float speed = 5.0f;
    
    void update(float dt, const InputState& input) {
        const float step = speed * dt;
        position.x += (static_cast<int>(input.right) - static_cast<int>(input.left)) * step;
        position.z += (static_cast<int>(input.up) - static_cast<int>(input.down)) * step;
    }
};

struct Scene {
    static constexpr float CAMERA_HEIGHT = 5.0f;
    static constexpr float CAMERA_DISTANCE = 6.0f;
    
    Player player;
    
    // Cámara en 3ª persona siguiendo a la cápsula
    DirectX::XMMATRIX getViewMatrix() const {
        const DirectX::XMFLOAT3& p = player.position;
        // El punto de mira (at) se sitúa ligeramente elevado sobre el centro de la cápsula
        const DirectX::XMVECTOR at = DirectX::XMVectorSet(p.x, p.y + 0.7f, p.z, 1.0f);
        // La cámara (eye) se coloca detrás y arriba del jugador
        const DirectX::XMVECTOR eye = DirectX::XMVectorSet(p.x, p.y + CAMERA_HEIGHT, p.z - CAMERA_DISTANCE, 1.0f);
        const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return DirectX::XMMatrixLookAtLH(eye, at, up);
    }
};