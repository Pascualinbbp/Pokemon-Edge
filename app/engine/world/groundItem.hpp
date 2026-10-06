#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"

// Objeto suelto en el mundo: no es sólido; lo recoge el jugador o su pokémon al pasar cerca y se desvanece.
class GroundItem {
    public:
    static constexpr float SIZE = 0.45f;
    static constexpr float INTERACT_RANGE = 1.8f; // distancia del jugador para recogerlo
    static constexpr float FADE_TIME = 0.35f;
    static constexpr float BOB_HEIGHT = 0.12f;

    Physics::Body body;

    GroundItem(int spot, int itemId, int quantity, const DirectX::XMFLOAT3& position)
        : m_spot(spot), m_itemId(itemId), m_quantity(quantity) {
        body.position = position; // sin colisión: el suelo lo gestiona la física común
    }

    int spot() const { return m_spot; }
    int itemId() const { return m_itemId; }
    int quantity() const { return m_quantity; }
    float reach() const { return INTERACT_RANGE; }
    bool available() const { return !m_taken; }
    bool finished() const { return m_taken && m_fade >= FADE_TIME; }

    void take() { m_taken = true; }

    void update(float dt) {
        m_time += dt;
        if (m_taken) m_fade += dt;
    }

    float bob() const { return (std::sin(m_time * 2.5f) * 0.5f + 0.5f) * BOB_HEIGHT; }
    float spin() const { return m_time * 1.2f; }
    float scale() const { return m_taken ? (std::max)(0.0f, 1.0f - m_fade / FADE_TIME) : 1.0f; }

    private:
    int m_spot;
    int m_itemId;
    int m_quantity;
    bool m_taken = false;
    float m_time = 0.0f;
    float m_fade = 0.0f;
};
