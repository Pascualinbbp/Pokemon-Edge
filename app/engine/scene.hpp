#pragma once
#include <DirectXMath.h>
#include "input.hpp"

using namespace DirectX;

struct Player {
    XMFLOAT3 position = { 0.0f, 1.0f, 0.0f }; // Empezamos a altura 1 para no hundirnos
    float speed = 5.0f;

    void update(float dt, const InputState& input) {
        if (input.up) position.z += speed * dt;
        if (input.down) position.z -= speed * dt;
        if (input.left) position.x -= speed * dt;
        if (input.right) position.x += speed * dt;
    }
};

struct Scene {
    Player player;

    // Cámara con vista isométrica/cenital (estilo Pokemon)
    XMMATRIX getViewMatrix() const {
        XMVECTOR eye = XMVectorSet(player.position.x, 8.0f, player.position.z - 8.0f, 1.0f);
        XMVECTOR at = XMVectorSet(player.position.x, player.position.y, player.position.z, 1.0f);
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return XMMatrixLookAtLH(eye, at, up);
    }
};