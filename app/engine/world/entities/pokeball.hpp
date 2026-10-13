#pragma once
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/body.hpp"

// Aspecto de cada tipo de pokéball (por id de la base de datos). Es el único sitio donde se define su color.
namespace PokeballStyle {
    inline DirectX::XMFLOAT3 color(int typeId) {
        switch (typeId) {
            case 1:  return { 0.86f, 0.16f, 0.16f }; // Poké Ball: roja
            case 2:  return { 0.16f, 0.39f, 0.90f }; // Super Ball: azul
            case 3:  return { 0.96f, 0.80f, 0.16f }; // Ultra Ball: amarilla
            case 4:  return { 0.62f, 0.26f, 0.85f }; // Starter Ball: morada (solo la lleva el pokémon inicial)
            default: return { 0.70f, 0.70f, 0.70f };
        }
    }
}

// Pokéball lanzada: una esfera pequeña con física propia (sin botes: al tocar el suelo rueda, sigue las cuestas y se frena).
struct Pokeball {
    static constexpr float RADIUS      = 0.15f;
    static constexpr float LIFETIME    = 8.0f;  // segundos hasta desaparecer
    static constexpr float DRAG        = 1.6f;
    static constexpr float GRAVITY_SCALE = 0.5f; // cae más despacio que el resto: la trayectoria es más tensa
    static constexpr float CONTACT_MARGIN = 0.03f; // holgura para detectar el contacto con un pokémon tras la colisión

    Physics::Body body;
    float age = 0.0f;
    bool spent = false;                             // alcanzó a un pokémon: se gasta aunque falle la captura
    int typeId = -1;                                // tabla pokeball: el tipo de bola lanzada
    float captureMultiplier = 1.0f;                 // del tipo de bola lanzada
    DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f }; // PokeballStyle::color del tipo lanzado
    float yaw = 0.0f;                               // orientación del modelo: hacia donde avanza
    float spin = 0.0f;                              // giro acumulado al rodar o volar (rad)

    Pokeball() {
        body.gravityScale = GRAVITY_SCALE;
        body.groundOffset = RADIUS; // 'position' es el centro de la esfera
        body.rolls = true;
        body.groundDrag = DRAG;
        body.collisionRadius = RADIUS;
        body.collisionHeight = RADIUS * 2.0f;
        body.hitsCreatures = false; // no rebota sobre los pokémon: los captura al tocarlos (hitBy)
        body.onGround = false;
    }

    bool expired() const { return age >= LIFETIME; }

    // Orienta el modelo según el movimiento: mira hacia donde avanza y gira sobre sí mismo tanto como recorre.
    void roll(float dt) {
        const float flat = std::hypot(body.velocity.x, body.velocity.z);
        if (flat > 0.3f) yaw = std::atan2(body.velocity.x, body.velocity.z);
        spin += std::sqrt(flat * flat + body.velocity.y * body.velocity.y) * dt / RADIUS;
    }
};
