#pragma once
#include <algorithm>
#include <DirectXMath.h>
#include "../core/input.hpp"
#include "camera.hpp"
#include "player.hpp"

struct Scene {
    static constexpr float HALF_SIZE = 40.0f; // el suelo va de -HALF_SIZE a +HALF_SIZE en X y Z

    Player player;
    Camera camera;

    void update(float dt, const InputState& input) {
        camera.rotate(input.lookX, input.lookY);
        player.update(dt, input, camera.yaw());
        player.position.x = (std::clamp)(player.position.x, -HALF_SIZE, HALF_SIZE);
        player.position.z = (std::clamp)(player.position.z, -HALF_SIZE, HALF_SIZE);
    }

    DirectX::XMMATRIX getViewMatrix() const {
        return camera.viewMatrix(player.position);
    }
};