#pragma once
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"

// Profesor Oak: el primer personaje no jugador. Está fijo junto al punto de aparición, mirando hacia el jugador; al hablar con él
// (si aún no hay pokémon) se abre la elección del inicial.
class Professor {
    public:
    static constexpr const char* NAME = "Profesor Oak";
    static constexpr float WIDTH = 0.6f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float INTERACT_RANGE = 2.2f; // distancia desde el borde para hablar con él
    static constexpr DirectX::XMFLOAT3 POSITION = { 2.6f, Physics::World::START_HEIGHT, 4.2f };

    float reach() const { return INTERACT_RANGE + WIDTH * 0.5f; }
    // Giro hacia el punto dado (el jugador): el modelo mira a +Z con yaw 0.
    static float yawToward(float x, float z) { return std::atan2(x - POSITION.x, z - POSITION.z); }

    Physics::World::Box solid() const { return { { POSITION.x, POSITION.y + HEIGHT * 0.5f, POSITION.z }, { WIDTH * 0.5f, HEIGHT * 0.5f, WIDTH * 0.5f } }; }

    Physics::Body body = makeBody(); // posición para el interaccionable genérico

    private:
    static Physics::Body makeBody() {
        Physics::Body b;
        b.position = POSITION;
        return b;
    }
};
