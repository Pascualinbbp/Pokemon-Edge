#pragma once
#include <DirectXMath.h>
#include "../physics/body.hpp"

// Pokéball lanzada: una esfera pequeña con física propia (bota y rueda al caer al suelo).
struct Pokeball {
    static constexpr float RADIUS      = 0.15f;
    static constexpr float LIFETIME    = 8.0f;  // segundos hasta desaparecer
    static constexpr float RESTITUTION = 0.45f;
    static constexpr float DRAG        = 2.0f;
    static constexpr float SHADOW      = 0.22f;

    Physics::Body body;
    float age = 0.0f;

    Pokeball() {
        body.gravityScale = 1.0f;
        body.groundOffset = RADIUS; // 'position' es el centro de la esfera
        body.restitution = RESTITUTION;
        body.groundDrag = DRAG;
        body.shadowRadius = SHADOW;
        body.onGround = false;
    }

    bool expired() const { return age >= LIFETIME; }
};
