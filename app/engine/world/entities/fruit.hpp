#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"

// Fruto caído de un árbol (coco de palmera, plátano de árbol de jungla). Cae, rueda por el suelo con su inercia y, si nadie lo coge,
// se pudre pasado un buen rato. Lo recogen el jugador (tiene prioridad), su acompañante o un pokémon salvaje (que se lo come).
class Fruit {
    public:
    enum class Kind { COCONUT, BANANA };
    static constexpr float RADIUS         = 0.13f;
    static constexpr float INTERACT_RANGE = 1.8f;
    static constexpr float LIFETIME       = 420.0f; // segundos en el suelo antes de pudrirse
    static constexpr float FADE_TIME      = 0.35f;
    static constexpr float ROT_FADE       = 6.0f;   // últimos segundos de vida: se encoge

    Physics::Body body;

    Fruit(Kind kind, int id, const DirectX::XMFLOAT3& from, float vx, float vz) : m_kind(kind), m_id(id) {
        body.position = from;
        body.velocity = { vx, 0.0f, vz };
        body.groundOffset = RADIUS;
        body.collisionRadius = RADIUS;
        body.collisionHeight = RADIUS * 2.0f;
        body.restitution = kind == Kind::COCONUT ? 0.35f : 0.12f;
        body.groundDrag = kind == Kind::COCONUT ? 0.55f : 1.6f; // el coco rueda lejos; el plátano se queda cerca
        body.rolls = kind == Kind::COCONUT;
    }

    Kind kind() const { return m_kind; }
    int id() const { return m_id; }
    float reach() const { return INTERACT_RANGE; }
    bool available() const { return !m_taken && m_age < LIFETIME; }
    bool finished() const { return (m_taken && m_fade >= FADE_TIME) || m_age >= LIFETIME; }
    void take() { m_taken = true; }
    float spin() const { return m_roll; }
    bool settled() const { return body.onGround; }

    void update(float dt) {
        if (body.onGround) {
            m_age += dt;
            m_roll += std::hypot(body.velocity.x, body.velocity.z) * dt / RADIUS; // gira según avanza
        } else m_age += dt * 0.25f;
        if (m_taken) m_fade += dt;
    }

    float scale() const {
        if (m_taken) return (std::max)(0.0f, 1.0f - m_fade / FADE_TIME);
        const float left = LIFETIME - m_age;
        return left < ROT_FADE ? (std::max)(0.0f, left / ROT_FADE) : 1.0f;
    }

    private:
    Kind m_kind;
    int m_id;
    bool m_taken = false;
    float m_age = 0.0f;
    float m_fade = 0.0f;
    float m_roll = 0.0f;
};
