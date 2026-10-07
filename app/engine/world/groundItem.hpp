#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"

// Objeto suelto en el mundo: una semiesfera blanca apoyada en el suelo que brilla un poco, con destellos en forma de
// estrella a su alrededor. No es sólida; la recoge el jugador o su pokémon al pasar cerca y se desvanece.
// Qué da se sortea al recogerla (tabla ground_item).
class GroundItem {
    public:
    static constexpr float RADIUS = 0.32f;        // radio de la semiesfera
    static constexpr int SPARKLES = 4;            // destellos a su alrededor
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

    // Brillo de la semiesfera (0..1), que late despacio.
    float glow() const { return 0.3f + 0.25f * pulse(0.0f); }

    // Destello 'index': posición, tamaño y giro. Orbitan cerca del suelo y se encienden y apagan por turnos.
    void sparkle(int index, DirectX::XMFLOAT3& position, float& size, float& spin) const {
        const float phase = static_cast<float>(index) / static_cast<float>(SPARKLES);
        const float angle = phase * 6.2831853f + m_time * 0.6f;
        const float reach = RADIUS * (1.35f + 0.25f * pulse(phase * 6.0f));
        position = { body.position.x + std::cos(angle) * reach, body.position.y + RADIUS * (0.5f + 0.9f * pulse(phase * 9.0f + 1.0f)), body.position.z + std::sin(angle) * reach };
        size = (0.05f + 0.13f * pulse(phase * 12.0f + 2.0f)) * scale();
        spin = m_time * 2.0f + static_cast<float>(index);
    }

    float scale() const { return m_taken ? (std::max)(0.0f, 1.0f - m_fade / FADE_TIME) : 1.0f; }

    private:
    int m_spot;
    float pulse(float phase) const { return std::sin(m_time * 2.2f + phase) * 0.5f + 0.5f; }

    bool m_taken = false;
    float m_time = 0.0f;
    float m_fade = 0.0f;
};
