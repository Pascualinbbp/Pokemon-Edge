#pragma once
#include <DirectXMath.h>
#include "../core/input.hpp"
#include "camera.hpp"
#include "player.hpp"

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