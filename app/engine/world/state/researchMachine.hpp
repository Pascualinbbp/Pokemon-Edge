#pragma once
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"

// Máquina de investigación: objeto fijo del mundo y sólido. Al usarla se analizan los pokémon capturados desde la
// última vez (ver PokemonStorage::analyze).
class ResearchMachine {
    public:
    static constexpr float WIDTH = 1.5f;
    static constexpr float DEPTH = 1.0f;
    static constexpr float HEIGHT = 1.7f;
    static constexpr float INTERACT_RANGE = 1.8f; // distancia desde el borde para poder usarla
    static constexpr DirectX::XMFLOAT3 POSITION = { -3.0f, 0.0f, 3.0f };
    static constexpr float YAW = 0.6f;

    float reach() const { return INTERACT_RANGE + WIDTH * 0.5f; }

    Physics::World::Box solid() const {
        const float half = (WIDTH > DEPTH ? WIDTH : DEPTH) * 0.5f;
        return { { POSITION.x, HEIGHT * 0.5f, POSITION.z }, { half, HEIGHT * 0.5f, half } };
    }

    // Posición para el interaccionable genérico (nearestIn usa body.position).
    Physics::Body body = makeBody();

    private:
    static Physics::Body makeBody() {
        Physics::Body b;
        b.position = POSITION;
        return b;
    }
};
