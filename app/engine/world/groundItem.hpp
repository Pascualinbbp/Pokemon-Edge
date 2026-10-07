#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"

// Objeto suelto en el mundo, como los de los juegos de Pokémon: una bola de objeto quieta en el suelo que brilla un poco
// y de la que salta una chispa. No es sólida; la recoge el jugador o su pokémon al pasar cerca y se desvanece.
// Qué da se sortea al recogerla (tabla ground_item).
class GroundItem {
    public:
    static constexpr float SIZE = 1.5f;           // escala de la bola (1 = tamaño de una pokéball lanzada)
    static constexpr float LIFT = 0.25f;          // altura del centro de la bola sobre el suelo
    static constexpr float INTERACT_RANGE = 1.8f; // distancia del jugador para recogerla
    static constexpr float FADE_TIME = 0.35f;

    Physics::Body body;

    GroundItem(int spot, const DirectX::XMFLOAT3& position) : m_spot(spot) { body.position = position; }

    int spot() const { return m_spot; }
    float reach() const { return INTERACT_RANGE; }
    bool available() const { return !m_taken; }
    bool finished() const { return m_taken && m_fade >= FADE_TIME; }

    void take() { m_taken = true; }
    void update(float dt) {
        m_time += dt;
        if (m_taken) m_fade += dt;
    }

    // Brillo de la bola y tamaño de la chispa (0..1), que laten despacio.
    float glow() const { return 0.3f + 0.25f * pulse(0.0f); }
    float sparkle() const { return pulse(1.7f); }
    float spin() const { return m_time * 1.5f; }

    float scale() const { return m_taken ? (std::max)(0.0f, 1.0f - m_fade / FADE_TIME) : 1.0f; }

    private:
    int m_spot;
    float pulse(float phase) const { return std::sin(m_time * 2.2f + phase) * 0.5f + 0.5f; }

    bool m_taken = false;
    float m_time = 0.0f;
    float m_fade = 0.0f;
};
