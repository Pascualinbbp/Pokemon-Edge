#pragma once
#include <cmath>
#include <DirectXMath.h>
#include "../core/input.hpp"
#include "camera.hpp"

struct Player {
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f }; // a nivel del suelo (y = 0)
    float speed = 5.0f;

    // El movimiento es relativo a hacia dónde mira la cámara (cameraYaw).
    void update(float dt, const InputState& input, float cameraYaw) {
        const float forward = static_cast<float>(input.up) - static_cast<float>(input.down);
        const float strafe  = static_cast<float>(input.right) - static_cast<float>(input.left);
        if (forward == 0.0f && strafe == 0.0f) return;

        const float s = std::sin(cameraYaw);
        const float c = std::cos(cameraYaw);
        // adelante = (s, c), derecha = (c, -s): base ortonormal, así que la longitud es sqrt(f² + s²).
        const float dirX = forward * s + strafe * c;
        const float dirZ = forward * c - strafe * s;
        const float step = speed * dt / std::sqrt(forward * forward + strafe * strafe); // diagonal normalizada

        position.x += dirX * step;
        position.z += dirZ * step;
    }
};

struct Scene {
    Player player;
    Camera camera;

    void update(float dt, const InputState& input) {
        camera.rotate(input.mouseDX, input.mouseDY);
        player.update(dt, input, camera.yaw());
    }

    DirectX::XMMATRIX getViewMatrix() const {
        return camera.viewMatrix(player.position);
    }
};