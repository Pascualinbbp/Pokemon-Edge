#pragma once
#include <algorithm>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"

// Objeto suelto en el mundo: una mini estrella quieta en el suelo. No es sólida; la recoge el jugador o su pokémon al
// pasar cerca y se desvanece. Qué da se sortea al recogerla (tabla ground_item).
class GroundItem {
    public:
    static constexpr float SIZE = 0.4f;
    static constexpr float LIFT = 0.25f;          // altura de la estrella sobre el suelo
    static constexpr float INTERACT_RANGE = 1.8f; // distancia del jugador para recogerla
    static constexpr float FADE_TIME = 0.35f;

    Physics::Body body;

    GroundItem(int spot, const DirectX::XMFLOAT3& position) : m_spot(spot) { body.position = position; }

    int spot() const { return m_spot; }
    float reach() const { return INTERACT_RANGE; }
    bool available() const { return !m_taken; }
    bool finished() const { return m_taken && m_fade >= FADE_TIME; }

    void take() { m_taken = true; }
    void update(float dt) { if (m_taken) m_fade += dt; }

    float scale() const { return m_taken ? (std::max)(0.0f, 1.0f - m_fade / FADE_TIME) : 1.0f; }

    private:
    int m_spot;
    bool m_taken = false;
    float m_fade = 0.0f;
};
