#pragma once
#include <DirectXMath.h>
#include "../core/input.hpp"
#include "../physics/physicsWorld.hpp"
#include "camera.hpp"
#include "player.hpp"

struct Scene {
    static constexpr float HALF_SIZE = Physics::World::HALF_SIZE;

    Physics::World world;
    Player player;
    Camera camera;

    void update(float dt, const InputState& input) {
        camera.rotate(input.lookX, input.lookY);
        player.update(dt, input, camera.yaw(), world);
    }

    // Recorre todos los cuerpos de la escena (hoy solo el jugador; después NPCs, pokémon, objetos...).
    // Es el único sitio que hay que ampliar al añadir entidades: sombras y física lo usan.
    template <typename Fn>
    void forEachBody(Fn&& fn) const {
        fn(player.body);
    }

    DirectX::XMMATRIX getViewMatrix() const {
        return camera.viewMatrix(player.body.position);
    }
};