#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"

// Aspecto de cada tipo de nodo (por id de la base de datos). Es el único sitio donde se define su forma y color.
namespace ResourceStyle {
    enum class Shape { TREE, ROCK, BUSH };

    struct Look {
        Shape shape;
        DirectX::XMFLOAT3 body;   // tronco / roca
        DirectX::XMFLOAT3 accent; // copa / vetas del mineral
        bool specks;              // la roca lleva vetas del color de 'accent'
        float half;               // semilado de la hitbox
        float height;             // altura de la hitbox
    };

    inline Look look(int typeId) {
        switch (typeId) {
            case 1:  return { Shape::TREE, { 0.40f, 0.26f, 0.13f }, { 0.18f, 0.55f, 0.20f }, false, 0.35f, 3.8f }; // árbol
            case 3:  return { Shape::ROCK, { 0.50f, 0.48f, 0.47f }, { 0.88f, 0.55f, 0.35f }, true,  0.85f, 1.1f }; // mena de hierro
            case 5:  return { Shape::ROCK, { 0.45f, 0.47f, 0.52f }, { 0.45f, 0.90f, 0.98f }, true,  0.85f, 1.1f }; // veta de diamante
            case 6:  return { Shape::BUSH, { 0.20f, 0.50f, 0.22f }, { 0.85f, 0.18f, 0.25f }, true,  0.55f, 0.9f }; // arbusto de bayas
            case 7:  return { Shape::BUSH, { 0.22f, 0.48f, 0.20f }, { 0.98f, 0.80f, 0.18f }, true,  0.55f, 0.9f }; // arbusto dorado
            case 4:  return { Shape::ROCK, { 0.42f, 0.42f, 0.45f }, { 0.07f, 0.07f, 0.08f }, true,  0.85f, 1.1f }; // filón de carbón
            default: return { Shape::ROCK, { 0.56f, 0.56f, 0.59f }, { 0.56f, 0.56f, 0.59f }, false, 0.85f, 1.1f }; // roca
        }
    }
}

// Nodo de recolección del mundo: sólido y de dos tipos.
//  - Extraíble (árbol, roca, mena...): se golpea varias veces con su habilidad y al agotarse se desvanece.
//  - Planta (arbusto...): crece sola; regarla lo acelera. Una vez crecida se recoge de un golpe y se desvanece.
class ResourceNode {
    public:
    static constexpr float INTERACT_RANGE = 1.6f; // distancia desde el borde de su hitbox para poder golpearlo
    static constexpr float HIT_COOLDOWN = 0.4f;   // segundos mínimos entre golpes
    static constexpr float SHAKE_TIME   = 0.3f;
    static constexpr float SHAKE_AMOUNT = 0.07f;
    static constexpr float FADE_TIME    = 0.6f;
    static constexpr float TEAM_WINDOW  = 1.5f;   // segundos durante los que cuenta que el otro siga trabajándolo
    static constexpr float TEAM_BONUS   = 2.0f;   // velocidad al trabajarlo jugador y pokémon a la vez
    static constexpr float WATER_STEP   = 0.08f;  // fracción de crecimiento que da cada riego (con velocidad 1)
    static constexpr float YOUNG_SCALE  = 0.45f;  // tamaño de una planta recién brotada

    Physics::Body body;

    // growSeconds > 0 = planta, que aparece con 'growth' (0..1) ya crecido.
    ResourceNode(int spot, int typeIndex, int typeId, int hitsNeeded, float growSeconds, float growth,
                 const DirectX::XMFLOAT3& position, float yaw)
        : m_spot(spot), m_type(typeIndex), m_typeId(typeId), m_need(hitsNeeded), m_yaw(yaw),
          m_growSeconds(growSeconds), m_growth(growSeconds > 0.0f ? growth : 1.0f) {
        body.position = position;
    }

    int spot() const { return m_spot; }
    int typeIndex() const { return m_type; }
    int typeId() const { return m_typeId; }
    float yaw() const { return m_yaw; }
    bool depleted() const { return m_hits >= m_need; }
    bool plant() const { return m_growSeconds > 0.0f; }
    bool growing() const { return m_growth < 1.0f; } // planta que aún no se puede recoger
    float growth() const { return m_growth; }
    bool finished() const { return depleted() && m_fade >= FADE_TIME; }

    // Distancia máxima (desde el centro) a la que el jugador puede golpearlo.
    float reach() const { return INTERACT_RANGE + ResourceStyle::look(m_typeId).half; }

    // Jugador y pokémon lo están trabajando a la vez: ambos van más rápido.
    float teamBonus(bool byPlayer) const { return (byPlayer ? m_companionTimer : m_playerTimer) > 0.0f ? TEAM_BONUS : 1.0f; }

    // Un paso de trabajo del jugador o su pokémon: riega si es una planta que aún crece; si no, golpea.
    // 'speed': velocidad de quien trabaja (herramienta o pokémon); mayor = menos espera hasta el siguiente paso.
    // Devuelve true si cuenta (no está agotado ni en enfriamiento).
    bool work(bool byPlayer, float speed = 1.0f) {
        if (depleted() || m_cooldown > 0.0f) return false;
        (byPlayer ? m_playerTimer : m_companionTimer) = TEAM_WINDOW;
        const float rate = teamBonus(byPlayer) * speed;
        if (growing()) m_growth = (std::min)(1.0f, m_growth + WATER_STEP * rate);
        else ++m_hits;
        m_cooldown = HIT_COOLDOWN / rate;
        m_shake = SHAKE_TIME;
        return true;
    }

    // El pokémon está trabajándolo ahora mismo (aunque aún no haya completado un golpe).
    void companionWorking() { m_companionTimer = TEAM_WINDOW; }

    void update(float dt) {
        m_cooldown = (std::max)(0.0f, m_cooldown - dt);
        m_shake = (std::max)(0.0f, m_shake - dt);
        if (growing()) m_growth = (std::min)(1.0f, m_growth + dt / m_growSeconds);
        m_playerTimer = (std::max)(0.0f, m_playerTimer - dt);
        m_companionTimer = (std::max)(0.0f, m_companionTimer - dt);
        if (depleted()) m_fade += dt;
    }

    // Sacudida lateral tras un golpe (con amortiguación).
    float shakeOffset() const { return std::sin(m_shake * 55.0f) * SHAKE_AMOUNT * (m_shake / SHAKE_TIME); }

    float scale() const {
        if (!depleted()) return plant() ? YOUNG_SCALE + (1.0f - YOUNG_SCALE) * m_growth : 1.0f;
        const float t = std::clamp(m_fade / FADE_TIME, 0.0f, 1.0f);
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }

    // Sólido mientras no se agota.
    Physics::World::Box solid() const {
        const ResourceStyle::Look look = ResourceStyle::look(m_typeId);
        return { { body.position.x, body.position.y + look.height * 0.5f, body.position.z }, { look.half, look.height * 0.5f, look.half } };
    }

    private:
    int m_spot;
    int m_type;
    int m_typeId;
    int m_need;
    float m_yaw;
    float m_growSeconds;
    float m_growth;                // 0..1; las no plantas siempre 1
    int m_hits = 0;
    float m_cooldown = 0.0f;
    float m_shake = 0.0f;
    float m_fade = 0.0f;
    float m_playerTimer = 0.0f;    // segundos que faltan para dar por parado al jugador
    float m_companionTimer = 0.0f; // ídem para el pokémon
};
