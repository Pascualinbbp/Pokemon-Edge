#pragma once
#include <DirectXMath.h>
#include "../physics/body.hpp"

// Pokéball lanzada: una esfera pequeña con física propia (bota y rueda al caer al suelo).
struct Pokeball {
    static constexpr float RADIUS      = 0.15f;
    static constexpr float LIFETIME    = 8.0f;  // segundos hasta desaparecer
    static constexpr float RESTITUTION = 0.45f;
    static constexpr float DRAG        = 2.0f;
    static constexpr float GRAVITY_SCALE = 0.5f; // cae más despacio que el resto: la trayectoria es más tensa

    Physics::Body body;
    float age = 0.0f;
    float captureMultiplier = 1.0f;                 // del tipo de bola lanzada
    DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f }; // del tipo de bola lanzada

    Pokeball() {
        body.gravityScale = GRAVITY_SCALE;
        body.groundOffset = RADIUS; // 'position' es el centro de la esfera
        body.restitution = RESTITUTION;
        body.groundDrag = DRAG;
        body.collisionRadius = RADIUS;
        body.collisionHeight = RADIUS * 2.0f;
        body.onGround = false;
    }

    bool expired() const { return age >= LIFETIME; }
};
