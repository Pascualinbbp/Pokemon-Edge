#pragma once
#include <cmath>
#include <string>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"

// Personaje no jugador junto al inicio: entrenadores (de prueba) o el tendero: fijo, mirando al jugador. Al hablar con él propone un combate. Cada entrenador tiene su estilo:
// en tiempo real (igual que contra los salvajes) o por turnos (combate integrado con menú).
class Trainer {
    public:
    enum class Style { REAL_TIME, TURN_BASED, DOUBLES, SHOP }; // DOUBLES: por turnos 2 contra 2; SHOP: no combate, es el tendero

    static constexpr float WIDTH = 0.6f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float INTERACT_RANGE = 2.4f;

    Trainer() = default;
    Trainer(std::string name, Style style, float x, float z, DirectX::XMFLOAT3 outfit) : name(std::move(name)), style(style), outfit(outfit) {
        body.position = { x, Physics::World::START_HEIGHT, z };
    }

    std::string name;
    Style style = Style::REAL_TIME;
    DirectX::XMFLOAT3 outfit = { 0.8f, 0.2f, 0.2f }; // color de la ropa
    Physics::Body body;
    bool gym = false;        // líder de gimnasio: al derrotarlo da una medalla
    bool badgeGiven = false; // ya la ha dado (en esta sesión)
    bool busy = false; // en pleno duelo: no acepta otro

    float reach() const { return INTERACT_RANGE + WIDTH * 0.5f; }
    float yawToward(float x, float z) const { return std::atan2(x - body.position.x, z - body.position.z); }
    Physics::World::Box solid() const { return { { body.position.x, body.position.y + HEIGHT * 0.5f, body.position.z }, { WIDTH * 0.5f, HEIGHT * 0.5f, WIDTH * 0.5f } }; }
};
