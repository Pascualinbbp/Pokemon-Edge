#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"

// Aspecto de cada rareza de cofre. Es el único sitio donde se define (el color no vive en la base de datos).
namespace ChestStyle {
    struct Look {
        DirectX::XMFLOAT3 lid;   // color de la tapa
        float glow;              // brillo propio de la tapa (0 = solo iluminada)
        int sparkles;            // chispas al abrirlo
    };

    inline Look look(int rarity) {
        switch (rarity) {
            case 2:  return { { 0.20f, 0.45f, 0.95f }, 0.15f, 10 }; // raro: azul
            case 3:  return { { 0.65f, 0.28f, 0.92f }, 0.28f, 16 }; // épico: morado
            default: return { { 0.60f, 0.40f, 0.22f }, 0.00f, 6 };  // común: madera
        }
    }

    inline constexpr DirectX::XMFLOAT3 BODY = { 0.42f, 0.27f, 0.14f };
    inline constexpr DirectX::XMFLOAT3 LOCK = { 0.95f, 0.78f, 0.22f };
}

// Cofre del mundo: sólido, con la física común. Al abrirlo levanta la tapa, suelta chispas y se desvanece.
class Chest {
    public:
    static constexpr float WIDTH  = 1.1f;
    static constexpr float DEPTH  = 0.75f;
    static constexpr float HEIGHT = 0.75f;
    static constexpr float BASE_RATIO = 0.55f;  // fracción de la altura que ocupa el cuerpo (el resto es la tapa)
    static constexpr float INTERACT_RANGE = 2.2f; // distancia del jugador al centro para poder abrirlo

    static constexpr float OPEN_TIME   = 0.6f;
    static constexpr float LID_ANGLE   = 1.9f;
    static constexpr float FADE_START  = 1.3f;
    static constexpr float TOTAL_TIME  = 1.9f;
    static constexpr float SPARKLE_LIFE = 1.0f;

    Physics::Body body;

    Chest(int spot, int typeIndex, int rarity, const DirectX::XMFLOAT3& position, float yaw)
        : m_spot(spot), m_type(typeIndex), m_rarity(rarity), m_yaw(yaw) {
        body.position = position; // sin radio de colisión: es el mundo quien lo ve a él (props), no al revés
    }

    int spot() const { return m_spot; } // punto de aparición del que salió
    int typeIndex() const { return m_type; }
    int rarity() const { return m_rarity; }
    float yaw() const { return m_yaw; }
    bool closed() const { return !m_opened; }
    bool finished() const { return m_opened && m_time >= TOTAL_TIME; }

    void open() {
        m_opened = true;
        m_time = 0.0f;
    }

    void update(float dt) { if (m_opened) m_time += dt; }

    DirectX::XMFLOAT3 center() const { return { body.position.x, body.position.y + HEIGHT * 0.5f, body.position.z }; }

    // Sólido mientras está cerrado (abierto ya no estorba).
    Physics::World::Box solid() const {
        const float r = (std::max)(WIDTH, DEPTH) * 0.5f; // caja alineada con los ejes que contiene al cofre girado
        return { center(), { r, HEIGHT * 0.5f, r } };
    }

    float lidAngle() const { return LID_ANGLE * ease(m_time / OPEN_TIME) * (m_opened ? 1.0f : 0.0f); }

    float scale() const {
        if (!m_opened || m_time < FADE_START) return 1.0f;
        return 1.0f - ease((m_time - FADE_START) / (TOTAL_TIME - FADE_START));
    }

    // Chispa 'index': posición, tamaño y giro. Escala 0 = no visible.
    void sparkle(int index, int count, DirectX::XMFLOAT3& position, float& size, float& spin) const {
        const float phase = static_cast<float>(index) / static_cast<float>(count);
        const float age = m_time - OPEN_TIME * 0.5f - phase * 0.5f;
        size = 0.0f;
        spin = 0.0f;
        position = center();
        if (!m_opened || age < 0.0f || age > SPARKLE_LIFE) return;

        const float t = age / SPARKLE_LIFE;
        const float angle = static_cast<float>(index) * 2.39996323f + age * 3.0f;
        const float reach = 0.25f + t * 0.9f;
        position = { body.position.x + std::cos(angle) * reach, body.position.y + HEIGHT * 0.8f + t * 2.0f, body.position.z + std::sin(angle) * reach };
        size = 0.2f * (std::min)(1.0f, t * 8.0f) * (1.0f - ease(t));
        spin = age * 6.0f + static_cast<float>(index);
    }

    private:
    static float ease(float x) {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    int m_spot;
    int m_type;
    int m_rarity;
    float m_yaw;
    bool m_opened = false;
    float m_time = 0.0f;
};
