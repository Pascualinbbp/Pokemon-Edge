#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"
#include "../../utils/core/randomUtil.hpp"
#include "captureRules.hpp"
#include "captureSequence.hpp"

// Objetivo de pruebas: un cubo inmóvil que hace de pokémon (el cubito amarillo marca hacia dónde mira).
// Una pokéball que lo toca lo captura: el resultado se decide al instante y CaptureSequence lo anima.
// Si se captura desaparece unos segundos y reaparece en su sitio con un porcentaje nuevo; si escapa, sale de la bola.
class CaptureTarget {
    public:
    static constexpr float SIZE          = 1.2f;  // arista del cubo
    static constexpr float HALF          = SIZE * 0.5f;
    static constexpr float RESPAWN_DELAY = 2.5f;  // segundos oculto antes de reaparecer

    enum class Event { NONE, CAPTURED, ESCAPED }; // se emite en el instante en que la animación revela el resultado

    Physics::Body body; // inmóvil, pero pasa por la misma física que el resto (suelo, paredes)

    explicit CaptureTarget(int slot = 0) : m_slot(slot % SLOT_COUNT) {
        body.collisionRadius = HALF;
        body.collisionHeight = SIZE;
        spawn();
    }

    bool hittable() const { return m_state == State::IDLE; }
    float yaw() const { return SPAWNS[m_slot][2]; }
    float baseChance() const { return m_baseChance; }

    // Especie de este pokémon salvaje (índice en GameData::species y su id). -1 = aún sin asignar: la escena la
    // asigna al aparecer, para que toda la lógica de elección esté en un solo sitio.
    int speciesIndex() const { return m_speciesIndex; }
    int speciesId() const { return m_speciesId; }
    void setSpecies(int index, int id) {
        m_speciesIndex = index;
        m_speciesId = id;
    }
    CaptureRules::Throw throwKind() const { return m_sequence.kind(); }

    void reroll() { m_baseChance = RandomUtil::range(CaptureRules::MIN_BASE, CaptureRules::MAX_BASE); }

    // Arista actual del cubo dibujado (se encoge al entrar en la bola).
    float scale() const {
        switch (m_state) {
            case State::IDLE:      return SIZE;
            case State::CAPTURING: return SIZE * m_sequence.pokemonScale();
            default:               return 0.0f;
        }
    }

    // Centro físico (fijo) y centro dibujado (se acerca a la bola mientras entra o sale de ella).
    DirectX::XMFLOAT3 center() const { return { body.position.x, body.position.y + HALF, body.position.z }; }

    DirectX::XMFLOAT3 drawCenter() const {
        const DirectX::XMFLOAT3 c = center();
        if (m_state != State::CAPTURING) return c;

        const DirectX::XMFLOAT3& b = m_sequence.ball().body.position;
        const float t = m_sequence.pokemonBlend();
        return { c.x + (b.x - c.x) * t, c.y + (b.y - c.y) * t, c.z + (b.z - c.z) * t };
    }

    // Animación de captura en curso (nullptr si no hay).
    const CaptureSequence* sequence() const { return m_state == State::CAPTURING ? &m_sequence : nullptr; }

    // Hitbox sólida (la que ve el jugador al caminar); desaparece en cuanto empieza la captura.
    Physics::World::Box solid() const { return { center(), { HALF, HALF, HALF }, false }; } // se salta por encima, pero no se puede estar encima

    // ¿Toca una esfera (centro, radio) al cubo?
    bool hitBy(const DirectX::XMFLOAT3& c, float radius) const {
        if (!hittable()) return false;
        const DirectX::XMFLOAT3 m = center();
        const float dx = c.x - (std::clamp)(c.x, m.x - HALF, m.x + HALF);
        const float dy = c.y - (std::clamp)(c.y, m.y - HALF, m.y + HALF);
        const float dz = c.z - (std::clamp)(c.z, m.z - HALF, m.z + HALF);
        return dx * dx + dy * dy + dz * dz <= radius * radius;
    }

    // Rayo contra el cubo (método de las losas). Devuelve la distancia al impacto.
    bool raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float& hitDistance) const {
        if (!hittable()) return false;
        const DirectX::XMFLOAT3 m = center();
        const float lo[3] = { m.x - HALF, m.y - HALF, m.z - HALF };
        const float hi[3] = { m.x + HALF, m.y + HALF, m.z + HALF };
        const float o[3] = { origin.x, origin.y, origin.z };
        const float d[3] = { dir.x, dir.y, dir.z };

        float tMin = 0.0f, tMax = 1.0e9f;
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(d[i]) < 1.0e-6f) {
                if (o[i] < lo[i] || o[i] > hi[i]) return false;
                continue;
            }
            float t0 = (lo[i] - o[i]) / d[i];
            float t1 = (hi[i] - o[i]) / d[i];
            if (t0 > t1) std::swap(t0, t1);
            tMin = (std::max)(tMin, t0);
            tMax = (std::min)(tMax, t1);
            if (tMin > tMax) return false;
        }
        hitDistance = tMin;
        return true;
    }

    // La bola ha tocado al pokémon: empieza la animación del resultado ya decidido.
    void beginCapture(const Pokeball& ball, const CaptureRules::Result& result) {
        m_state = State::CAPTURING;
        m_sequence.start(ball, result);
    }

    Event update(float dt, const Physics::World& world) {
        world.step(body, dt);

        Event event = Event::NONE;
        switch (m_state) {
            case State::IDLE:
                break;
            case State::CAPTURING:
                if (m_sequence.update(dt, world)) event = m_sequence.captured() ? Event::CAPTURED : Event::ESCAPED;
                if (m_sequence.finished()) {
                    if (m_sequence.captured()) {
                        m_state = State::HIDDEN;
                        m_timer = 0.0f;
                    } else {
                        m_state = State::IDLE; // escapó: ya está de nuevo en su sitio, con otro porcentaje
                        reroll();
                    }
                }
                break;
            case State::HIDDEN:
                m_timer += dt;
                if (m_timer >= RESPAWN_DELAY) spawn();
                break;
        }
        return event;
    }

    private:
    enum class State { IDLE, CAPTURING, HIDDEN };

    // Posición (x, z) y hacia dónde mira (yaw, 0 = +Z) de cada pokémon. El jugador empieza en el origen mirando a +Z.
    static constexpr float SPAWNS[][3] = {
        {   0.0f,  14.0f,  3.14159265f },  // de frente al jugador
        { -12.0f,  18.0f,  0.0f },         // de espaldas al jugador
        {  14.0f,  10.0f, -1.57079633f },  // mirando hacia el centro
        {  -8.0f, -14.0f,  3.14159265f },  // de espaldas al jugador
    };
    static constexpr int SLOT_COUNT = static_cast<int>(sizeof(SPAWNS) / sizeof(SPAWNS[0]));

    void spawn() {
        body.position = { SPAWNS[m_slot][0], 0.0f, SPAWNS[m_slot][1] };
        m_state = State::IDLE;
        m_timer = 0.0f;
        m_speciesIndex = m_speciesId = -1;
        reroll();
    }

    State m_state = State::IDLE;
    CaptureSequence m_sequence;
    float m_timer = 0.0f;
    float m_baseChance = 50.0f;
    int m_slot = 0;
    int m_speciesIndex = -1;
    int m_speciesId = -1;
};
