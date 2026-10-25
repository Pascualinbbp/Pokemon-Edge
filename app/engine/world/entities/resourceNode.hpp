#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"

// Aspecto de cada tipo de nodo (por id de la base de datos). Es el único sitio donde se define su forma y color.
namespace ResourceStyle {
    enum class Shape { TREE, CONIFER, PALM, JUNGLE, ACACIA, ROCK, BUSH, FLOWER, TULIP, SUNFLOWER, CORAL, KELP };

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
            case 8:  return { Shape::CONIFER, { 0.34f, 0.22f, 0.12f }, { 0.10f, 0.42f, 0.24f }, false, 0.35f, 3.8f }; // pino
            case 9:  return { Shape::TREE, { 0.86f, 0.84f, 0.78f }, { 0.52f, 0.72f, 0.22f }, false, 0.3f, 3.4f };   // abedul
            case 10: return { Shape::PALM, { 0.55f, 0.40f, 0.22f }, { 0.28f, 0.62f, 0.22f }, false, 0.3f, 3.8f };   // palmera
            case 11: return { Shape::TREE, { 0.30f, 0.20f, 0.16f }, { 0.98f, 0.68f, 0.80f }, false, 0.3f, 3.6f };   // cerezo en flor
            case 12: return { Shape::FLOWER, { 0.28f, 0.58f, 0.24f }, { 0.98f, 0.98f, 0.96f }, true, 0.25f, 0.55f };  // margarita (pétalos blancos)
            case 13: return { Shape::JUNGLE, { 0.30f, 0.22f, 0.14f }, { 0.08f, 0.40f, 0.12f }, false, 0.45f, 5.0f };  // árbol de jungla
            case 14: return { Shape::ACACIA, { 0.42f, 0.30f, 0.18f }, { 0.50f, 0.58f, 0.18f }, false, 0.3f, 4.2f };   // acacia
            case 15: return { Shape::BUSH, { 0.14f, 0.50f, 0.20f }, { 0.98f, 0.58f, 0.14f }, true, 0.6f, 1.0f };     // planta tropical
            case 16: return { Shape::TULIP, { 0.28f, 0.58f, 0.24f }, { 0.92f, 0.24f, 0.36f }, true, 0.25f, 0.6f };  // tulipán
            case 17: return { Shape::SUNFLOWER, { 0.30f, 0.56f, 0.22f }, { 0.99f, 0.82f, 0.14f }, true, 0.3f, 1.5f }; // girasol
            case 18: return { Shape::FLOWER, { 0.28f, 0.58f, 0.24f }, { 0.90f, 0.16f, 0.12f }, true, 0.25f, 0.5f };  // amapola
            case 19: return { Shape::ROCK, { 0.10f, 0.09f, 0.13f }, { 0.85f, 0.25f, 0.10f }, true, 0.85f, 1.1f };    // obsidiana (vetas de brasa)
            case 20: return { Shape::CORAL, { 0.95f, 0.45f, 0.50f }, { 0.98f, 0.70f, 0.30f }, false, 0.55f, 1.0f };  // coral
            case 21: return { Shape::KELP, { 0.15f, 0.45f, 0.22f }, { 0.30f, 0.65f, 0.28f }, false, 0.3f, 2.4f };    // alga
            case 22: return { Shape::ROCK, { 0.62f, 0.82f, 0.92f }, { 0.85f, 0.97f, 1.00f }, true, 0.85f, 1.1f };    // cristal de hielo
            case 23: return { Shape::ROCK, { 0.45f, 0.45f, 0.48f }, { 0.99f, 0.82f, 0.18f }, true, 0.85f, 1.1f };  // filón de oro
            case 4:  return { Shape::ROCK, { 0.42f, 0.42f, 0.45f }, { 0.07f, 0.07f, 0.08f }, true,  0.85f, 1.1f }; // filón de carbón
            default: return { Shape::ROCK, { 0.56f, 0.56f, 0.59f }, { 0.56f, 0.56f, 0.59f }, false, 0.85f, 1.1f }; // roca
        }
    }

    // Cómo crece cada forma al regenerarse (growth 0..1): el árbol es un tocón hasta STUMP_UNTIL y luego crece entero; la roca
    // común se regenera entera y la mena conserva su base (solo regenera las vetas).
    inline constexpr float STUMP_UNTIL = 0.2f;
    inline constexpr float STUMP_HEIGHT = 0.4f;
    inline constexpr float STUMP_HALF = 0.3f;

    inline bool isTree(Shape shape) { return shape == Shape::TREE || shape == Shape::CONIFER || shape == Shape::PALM || shape == Shape::JUNGLE || shape == Shape::ACACIA; }
    inline bool stump(const Look& look, float growth) { return isTree(look.shape) && growth < STUMP_UNTIL; }
    inline float treeScale(float growth) { return 0.25f + 0.75f * (growth - STUMP_UNTIL) / (1.0f - STUMP_UNTIL); }
    inline float rockScale(const Look& look, float growth) { return look.specks ? 1.0f : 0.3f + 0.7f * growth; }

    // Semilado y altura de la hitbox según su estado: el tocón y la roca pequeña también estorban.
    inline void hitbox(const Look& look, float growth, float& half, float& height) {
        half = look.half;
        height = look.height;
        if (stump(look, growth)) {
            half = STUMP_HALF;
            height = STUMP_HEIGHT;
        } else if (isTree(look.shape)) {
            const float k = treeScale(growth);
            half *= k;
            height *= k;
        } else if (look.shape == Shape::ROCK) {
            const float k = rockScale(look, growth);
            half *= k;
            height *= k;
        }
    }
}

// Nodo de recolección del mundo. Nunca desaparece: al agotarse vuelve a estar listo con el tiempo.
//  - Extraíble (árbol, roca, mena...): se golpea varias veces con su habilidad. Al agotarse deja el tocón (o la roca base) y
//    se regenera poco a poco; hasta que no termina no se puede volver a trabajar.
//  - Planta (arbusto...): crece sola; regarla lo acelera. Una vez crecida se recoge de un golpe: queda el arbusto sin frutos y
//    estos vuelven a crecer.
class ResourceNode {
    public:
    static constexpr float INTERACT_RANGE = 1.6f; // distancia desde el borde de su hitbox para poder golpearlo
    static constexpr float HIT_COOLDOWN = 0.4f;   // segundos mínimos entre golpes
    static constexpr float SHAKE_TIME   = 0.3f;
    static constexpr float SHAKE_AMOUNT = 0.07f;
    static constexpr float TEAM_WINDOW  = 1.5f;   // segundos durante los que cuenta que el otro siga trabajándolo
    static constexpr float TEAM_BONUS   = 2.0f;   // velocidad al trabajarlo jugador y pokémon a la vez
    static constexpr float VEIN_REACH   = 2.9f;   // las vetas de pared se alcanzan desde el centro del paso
    static constexpr float WATER_STEP   = 0.08f;  // fracción de crecimiento que da cada riego (con velocidad 1)

    Physics::Body body;

    // 'plant': se riega para acelerar su rebrote. 'regrowSeconds': tiempo en volver a estar listo. 'growth' (0..1): cuánto lleva.
    ResourceNode(int typeIndex, int typeId, int hitsNeeded, bool plant, float regrowSeconds, float growth,
                 const DirectX::XMFLOAT3& position, float yaw, bool waterable = true, bool ephemeral = false)
        : m_type(typeIndex), m_typeId(typeId), m_need(hitsNeeded), m_yaw(yaw), m_plant(plant),
          m_regrowSeconds((std::max)(1.0f, regrowSeconds)), m_growth(growth), m_waterable(waterable), m_ephemeral(ephemeral) {
        body.position = position;
    }

    // Una flor recogida desaparece: su hueco queda libre hasta que el hábitat haga brotar otra (respawn).
    bool gone() const { return m_gone; }
    float goneTime() const { return m_goneTime; }
    void respawn(int typeIndex, int typeId, int hitsNeeded, float growSeconds, const DirectX::XMFLOAT3& position, float yaw, bool underground) {
        m_type = typeIndex; m_typeId = typeId; m_need = hitsNeeded; m_yaw = yaw; m_regrowSeconds = (std::max)(1.0f, growSeconds);
        m_growth = 0.0f; m_hits = 0; m_gone = false; m_goneTime = 0.0f; m_harvested = false; m_plant = true; m_waterable = false; m_ephemeral = true;
        body.position = position;
        body.underground = underground;
    }

    int typeIndex() const { return m_type; }
    int typeId() const { return m_typeId; }
    float yaw() const { return m_yaw; }
    bool plant() const { return m_plant; }
    bool growing() const { return m_growth < 1.0f && !m_gone; } // aún no se puede recoger (o extraer)
    bool depleted() const { return m_gone || (!m_plant && growing()); } // extraíble agotado: nada que hacer hasta que se regenere
    float growth() const { return m_growth; }
    // ¿Se puede trabajar ahora? (no una flor o alga que aún crece sola, ni un nodo agotado)
    bool workable() const { return !depleted() && !(m_plant && growing() && !m_waterable); }

    // Distancia máxima (desde el centro) a la que el jugador puede golpearlo.
    float reach() const { return m_vein ? VEIN_REACH : INTERACT_RANGE + ResourceStyle::look(m_typeId).half; }

    // Veta incrustada en la pared de una galería: un parche de mineral a ras de roca (no estorba ni se pisa). 'yaw' mira hacia fuera de la pared.
    void makeVein(float yaw) { m_vein = true; m_yaw = yaw; }
    bool vein() const { return m_vein; }
    void setRegrowSeconds(float seconds) { m_regrowSeconds = (std::max)(1.0f, seconds); }

    // Jugador y pokémon lo están trabajando a la vez: ambos van más rápido.
    float teamBonus(bool byPlayer) const { return (byPlayer ? m_companionTimer : m_playerTimer) > 0.0f ? TEAM_BONUS : 1.0f; }

    // Un paso de trabajo del jugador o su pokémon: riega si es una planta que aún crece; si no, golpea.
    // 'speed': velocidad de quien trabaja (herramienta o pokémon); mayor = menos espera hasta el siguiente paso.
    // Devuelve true si cuenta (no está agotado ni en enfriamiento).
    bool work(bool byPlayer, float speed = 1.0f) {
        if (depleted() || m_cooldown > 0.0f) return false;
        if (m_plant && growing() && !m_waterable) return false; // las flores y algas crecen solas: no se riegan
        (byPlayer ? m_playerTimer : m_companionTimer) = TEAM_WINDOW;
        const float rate = teamBonus(byPlayer) * speed;
        if (growing()) m_growth = (std::min)(1.0f, m_growth + WATER_STEP * rate);
        else if (++m_hits >= m_need) {
            m_hits = 0;
            m_harvested = true;
            if (m_ephemeral) { m_gone = true; m_goneTime = 0.0f; }
            else m_growth = 0.0f; // queda el tocón / la base y empieza a regenerarse
        }
        m_cooldown = HIT_COOLDOWN / rate;
        m_shake = SHAKE_TIME;
        return true;
    }

    // true una sola vez tras el golpe que lo agota: quien lo trabaja cobra entonces el material.
    bool takeHarvest() {
        const bool harvested = m_harvested;
        m_harvested = false;
        return harvested;
    }

    // El pokémon está trabajándolo ahora mismo (aunque aún no haya completado un golpe).
    void companionWorking() { m_companionTimer = TEAM_WINDOW; }

    void update(float dt) {
        m_cooldown = (std::max)(0.0f, m_cooldown - dt);
        m_shake = (std::max)(0.0f, m_shake - dt);
        if (m_gone) m_goneTime += dt;
        else if (growing()) m_growth = (std::min)(1.0f, m_growth + dt / m_regrowSeconds);
        m_playerTimer = (std::max)(0.0f, m_playerTimer - dt);
        m_companionTimer = (std::max)(0.0f, m_companionTimer - dt);
    }

    // Sacudida lateral tras un golpe (con amortiguación).
    float shakeOffset() const { return std::sin(m_shake * 55.0f) * SHAKE_AMOUNT * (m_shake / SHAKE_TIME); }

    // Siempre sólido, también el tocón o la roca que se está regenerando (con el tamaño que tienen en ese momento).
    Physics::World::Box solid() const {
        float half, height;
        ResourceStyle::hitbox(ResourceStyle::look(m_typeId), m_growth, half, height);
        return { { body.position.x, body.position.y + height * 0.5f, body.position.z }, { half, height * 0.5f, half } };
    }

    private:
    int m_type;
    int m_typeId;
    int m_need;
    float m_yaw;
    bool m_plant;
    float m_regrowSeconds;
    float m_growth;                // 0..1; 1 = listo
    bool m_waterable = true;       // regarla acelera su crecimiento (solo arbustos)
    bool m_ephemeral = false;      // al recogerse desaparece
    bool m_vein = false;           // mineral incrustado en una pared
    bool m_gone = false;
    float m_goneTime = 0.0f;
    int m_hits = 0;
    bool m_harvested = false;
    float m_cooldown = 0.0f;
    float m_shake = 0.0f;
    float m_playerTimer = 0.0f;    // segundos que faltan para dar por parado al jugador
    float m_companionTimer = 0.0f; // ídem para el pokémon
};
