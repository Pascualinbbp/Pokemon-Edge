#pragma once
#include <DirectXMath.h>
#include "../core/input.hpp"

struct Player {
    DirectX::XMFLOAT3 position = { 0.0f, 1.0f, 0.0f };
    float speed = 5.0f;

    void update(float dt, const InputState& input) {
        const float step = speed * dt;
        position.x += (static_cast<int>(input.right) - static_cast<int>(input.left)) * step;
        position.z += (static_cast<int>(input.up) - static_cast<int>(input.down)) * step;
    }
};

struct Scene {
    static constexpr float CAMERA_HEIGHT = 8.0f;
    static constexpr float CAMERA_DISTANCE = 8.0f;

    Player player;

    // Cámara cenital/isométrica (estilo Pokémon)
    DirectX::XMMATRIX getViewMatrix() const {
        const DirectX::XMFLOAT3& p = player.position;
        const DirectX::XMVECTOR eye = DirectX::XMVectorSet(p.x, CAMERA_HEIGHT, p.z - CAMERA_DISTANCE, 1.0f);
        const DirectX::XMVECTOR at = DirectX::XMLoadFloat3(&p);
        const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return DirectX::XMMatrixLookAtLH(eye, at, up);
    }
};