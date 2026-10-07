#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"
#include "../../utils/core/randomUtil.hpp"
#include "captureRules.hpp"
#include "captureSequence.hpp"

// Pokémon salvaje (un cubo): vaga libremente cerca de su punto de aparición; el cubito amarillo marca hacia dónde mira.
// Una pokéball que lo toca lo captura: el resultado se decide al instante y CaptureSequence lo anima.
// Si se captura desaparece unos segundos y reaparece en su sitio con un porcentaje nuevo; si escapa, sale de la bola.
class CaptureTarget {
    public:
    static constexpr float SIZE          = 1.2f;  // arista del cubo
    static constexpr float HALF          = SIZE * 0.5f;
    static constexpr float RESPAWN_DELAY = 2.5f;  // segundos oculto antes de reaparecer
    static constexpr float WALK_SPEED    = 1.8f;  // m/s al vagar
    static constexpr float TURN_SPEED    = 6.0f;  // rad/s al girar hacia donde camina
    static constexpr float LEASH         = 9.0f;  // distancia máxima a su punto de aparición

    enum class Event { NONE, CAPTURED, ESCAPED }; // se emite en el instante en que la animación revela el resultado

    Physics::Body body; // pasa por la misma física que el resto (suelo, paredes)

    explicit CaptureTarget(int slot = 0) : m_slot(slot % SLOT_COUNT) {
        body.collisionRadius = HALF;
        body.collisionHeight = SIZE;
        spawn();
    }

    bool hittable() const { return m_state == State::IDLE; }
    float yaw() const { return m_yaw; }

    // Especie de este pokémon salvaje (índice en GameData::species y su id). -1 = aún sin asignar: la escena la
    // asigna al aparecer, para que toda la lógica de elección esté en un solo sitio.
    int speciesIndex() const { return m_speciesIndex; }
    int speciesId() const { return m_speciesId; }
    int level() const { return m_level; }
    bool shiny() const { return m_shiny; }
    int ballId() const { return m_ballId; } // pokéball con la que se está capturando
    void setSpecies(int index, int id, int level, bool shiny) {
        m_speciesIndex = index;
        m_speciesId = id;
        m_level = level;
        m_shiny = shiny;
    }
    CaptureRules::Throw throwKind() const { return m_sequence.kind(); }

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
        m_ballId = ball.typeId;
        m_sequence.start(ball, result);
    }

    Event update(float dt, const Physics::World& world) {
        Event event = Event::NONE;
        switch (m_state) {
            case State::IDLE:
                wander(dt);
                break;
            case State::CAPTURING:
                body.velocity.x = body.velocity.z = 0.0f;
                if (m_sequence.update(dt, world)) event = m_sequence.captured() ? Event::CAPTURED : Event::ESCAPED;
                if (m_sequence.finished()) {
                    if (m_sequence.captured()) {
                        m_state = State::HIDDEN;
                        m_timer = 0.0f;
                    } else {
                        m_state = State::IDLE; // escapó: ya está de nuevo en su sitio
                    }
                }
                break;
            case State::HIDDEN:
                m_timer += dt;
                if (m_timer >= RESPAWN_DELAY) spawn();
                break;
        }
        world.step(body, dt);
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

    // Alterna paradas y paseos en una dirección al azar; si se aleja demasiado de su punto, vuelve hacia él.
    void wander(float dt) {
        m_wanderTimer -= dt;
        if (m_wanderTimer <= 0.0f) {
            const float homeDx = SPAWNS[m_slot][0] - body.position.x, homeDz = SPAWNS[m_slot][1] - body.position.z;
            const bool tooFar = homeDx * homeDx + homeDz * homeDz > LEASH * LEASH;
            m_walking = tooFar || RandomUtil::roll(65.0f);
            m_wanderTimer = m_walking ? RandomUtil::range(1.5f, 3.5f) : RandomUtil::range(1.5f, 4.0f);
            if (m_walking) m_heading = tooFar ? std::atan2(homeDx, homeDz) : RandomUtil::range(-3.14159265f, 3.14159265f);
        }

        const float delta = DirectX::XMScalarModAngle(m_heading - m_yaw);
        if (m_walking) m_yaw += (std::clamp)(delta, -TURN_SPEED * dt, TURN_SPEED * dt);
        const bool go = m_walking && std::fabs(delta) < 0.5f;
        body.velocity.x = go ? std::sin(m_yaw) * WALK_SPEED : 0.0f;
        body.velocity.z = go ? std::cos(m_yaw) * WALK_SPEED : 0.0f;
    }

    void spawn() {
        body.position = { SPAWNS[m_slot][0], 0.0f, SPAWNS[m_slot][1] };
        body.velocity = { 0.0f, 0.0f, 0.0f };
        m_yaw = m_heading = SPAWNS[m_slot][2];
        m_walking = false;
        m_wanderTimer = RandomUtil::range(0.5f, 3.0f);
        m_state = State::IDLE;
        m_timer = 0.0f;
        m_speciesIndex = m_speciesId = -1;
        m_shiny = false;
    }

    State m_state = State::IDLE;
    CaptureSequence m_sequence;
    float m_timer = 0.0f;
    int m_slot = 0;
    float m_yaw = 0.0f;
    float m_heading = 0.0f;   // hacia dónde quiere caminar
    float m_wanderTimer = 0.0f;
    bool m_walking = false;
    int m_speciesIndex = -1;
    int m_speciesId = -1;
    int m_level = 1;
    bool m_shiny = false;
    int m_ballId = -1;
};
