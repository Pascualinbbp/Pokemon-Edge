#pragma once
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"

// Portal: máquina fija y sólida que lleva de un mapa a otro. Una está junto a la máquina de investigación del inicio (lleva a la granja, el mundo
// plano) y otra en el centro de la granja (vuelve al inicio). Al usarla la pantalla se cubre mientras se cambia de mapa (ver Scene::startWarp).
class Portal {
    public:
    static constexpr float WIDTH = 1.7f;
    static constexpr float HEIGHT = 2.4f;
    static constexpr float INTERACT_RANGE = 1.8f;
    static constexpr float EXIT_DISTANCE = 3.2f;  // a esta distancia por delante aparece quien sale por ella

    explicit Portal(const DirectX::XMFLOAT3& position = {}, float yaw = 0.0f) : yaw(yaw) { body.position = position; }

    float reach() const { return INTERACT_RANGE + WIDTH * 0.5f; }

    Physics::World::Box solid() const {
        const float half = WIDTH * 0.5f;
        return { { body.position.x, body.position.y + HEIGHT * 0.5f, body.position.z }, { half, HEIGHT * 0.5f, half } };
    }

    // Punto donde aparece quien sale por este portal: delante de él.
    DirectX::XMFLOAT3 exitPoint() const {
        return { body.position.x + std::sin(yaw) * EXIT_DISTANCE, body.position.y, body.position.z + std::cos(yaw) * EXIT_DISTANCE };
    }

    Physics::Body body;
    float yaw = 0.0f;
};
