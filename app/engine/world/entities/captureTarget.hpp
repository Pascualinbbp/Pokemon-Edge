#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"
#include "../../../utils/core/randomUtil.hpp"
#include "../rules/captureRules.hpp"
#include "../state/captureSequence.hpp"

// Pokémon salvaje (un cubo): vaga libremente cerca de su punto de aparición; el cubito amarillo marca hacia dónde mira.
// Una pokéball que lo toca lo captura: el resultado se decide al instante y CaptureSequence lo anima.
// Vive en un chunk (WildField): conserva su sitio y su especie aunque el jugador se aleje; solo se muestra y se simula
// mientras está cerca, y aparece/desaparece encogiéndose y creciendo. Si se captura o se derrota, el chunk lo renueva más tarde.
class CaptureTarget {
    public:
    static constexpr float SIZE          = 1.2f;  // arista del cubo
    static constexpr float HALF          = SIZE * 0.5f;
    static constexpr float REFILL_DELAY  = 90.0f; // segundos fuera de juego antes de que el chunk lo renueve
    static constexpr float LEAVE_TIME    = 7.0f;  // segundos que tarda en desaparecer al agotársele el tiempo en el mundo (se aleja mientras tanto)
    static constexpr float LEAVE_REFILL  = 20.0f; // segundos tras desaparecer hasta que su sitio puede recibir otro
    static constexpr float LIFETIME_MIN  = 480.0f; // tiempo en el mundo (a la vista) antes de marcharse
    static constexpr float LIFETIME_MAX  = 840.0f;
    static constexpr float APPEAR_TIME   = 1.4f;  // segundos que tarda en aparecer o desaparecer al entrar/salir del alcance
    static constexpr float SOLID_APPEAR  = 0.6f;  // a partir de este avance de aparición ya se puede golpear y es sólido
    static constexpr float WALK_SPEED    = 1.8f;  // m/s al vagar
    static constexpr float TURN_SPEED    = 6.0f;  // rad/s al girar hacia donde camina
    static constexpr float LEASH         = 6.0f;  // distancia máxima a su punto de aparición

    enum class Event { NONE, CAPTURED, ESCAPED }; // se emite en el instante en que la animación revela el resultado

    Physics::Body body; // pasa por la misma física que el resto (suelo, paredes)

    CaptureTarget() {
        body.collisionRadius = HALF;
        body.collisionHeight = SIZE;
    }

    // Lo coloca (o recoloca) en un punto del mundo, vivo, con la vida entera y sin especie asignada.
    void place(float x, float z, float yaw) {
        body.position = { x, 0.0f, z };
        body.velocity = { 0.0f, 0.0f, 0.0f };
        m_home = { x, z };
        m_yaw = m_heading = yaw;
        m_walking = false;
        m_wanderTimer = RandomUtil::range(0.5f, 3.0f);
        m_state = State::IDLE;
        m_timer = 0.0f;
        m_appear = 0.0f;
        m_speciesIndex = m_speciesId = -1;
        m_shiny = false;
        m_hp = 1.0f;
        m_leaving = false;
        m_age = 0.0f;
        m_lifetime = RandomUtil::range(LIFETIME_MIN, LIFETIME_MAX);
    }

    bool hittable() const { return m_state == State::IDLE && m_appear >= SOLID_APPEAR; }
    bool gone() const { return m_state == State::HIDDEN; }       // capturado o derrotado: espera a que el chunk lo renueve
    bool capturing() const { return m_state == State::CAPTURING; }
    bool shown() const { return m_state == State::CAPTURING || (m_state == State::IDLE && m_appear > 0.0f); } // cargado en el mundo (se simula y se dibuja)

    // Hace aparecer (show) o desaparecer el modelo poco a poco.
    void fade(float dt, bool show) {
        m_appear = std::clamp(m_appear + (show ? dt / APPEAR_TIME : -dt / (m_leaving ? LEAVE_TIME : APPEAR_TIME)), 0.0f, 1.0f);
        if (m_leaving && m_appear <= 0.0f && m_state == State::IDLE) { // se ha ido del todo: su sitio queda libre para otro
            m_state = State::HIDDEN;
            m_timer = REFILL_DELAY - LEAVE_REFILL;
            m_leaving = false;
        }
    }

    // Cuenta el tiempo que lleva en el mundo a la vista; true cuando ya le toca marcharse.
    bool lifetimeOver(float dt) {
        m_age += dt;
        return !m_leaving && m_age >= m_lifetime;
    }

    // Se marcha andando en sentido contrario al jugador mientras se desvanece poco a poco.
    void leave(float fromX, float fromZ) {
        m_leaving = true;
        m_heading = std::atan2(body.position.x - fromX, body.position.z - fromZ);
    }
    bool leaving() const { return m_leaving; }

    // Cuenta el tiempo fuera de juego; true cuando el chunk ya puede renovarlo.
    bool refillDue(float dt) {
        m_timer += dt;
        return m_timer >= REFILL_DELAY;
    }

    // Vida restante (1 = entera). La reducen los golpes del acompañante; al llegar a 0 queda derrotado y reaparece más tarde.
    float hpFraction() const { return m_hp; }
    // Quita una fracción de vida. Devuelve true si lo deja derrotado.
    bool damage(float fraction) {
        if (!hittable()) return false;
        m_hp = (std::max)(0.0f, m_hp - fraction);
        if (m_hp > 0.0f) return false;
        m_state = State::HIDDEN;
        m_timer = 0.0f;
        return true;
    }
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
            case State::IDLE:      return SIZE * m_appear * m_appear * (3.0f - 2.0f * m_appear); // suavizado
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
                return event;
        }
        world.step(body, dt);
        return event;
    }

    private:
    enum class State { IDLE, CAPTURING, HIDDEN };

    // Alterna paradas y paseos en una dirección al azar; si se aleja demasiado de su punto, vuelve hacia él.
    void wander(float dt) {
        if (m_leaving) { // se aleja sin pararse
            m_walking = true;
            m_wanderTimer = 1.0f;
        }
        m_wanderTimer -= dt;
        if (m_wanderTimer <= 0.0f) {
            const float homeDx = m_home.x - body.position.x, homeDz = m_home.y - body.position.z;
            const bool tooFar = homeDx * homeDx + homeDz * homeDz > LEASH * LEASH;
            m_walking = tooFar || RandomUtil::roll(65.0f);
            m_wanderTimer = m_walking ? RandomUtil::range(1.5f, 3.5f) : RandomUtil::range(1.5f, 4.0f);
            if (m_walking) m_heading = tooFar ? std::atan2(homeDx, homeDz) : RandomUtil::range(-3.14159265f, 3.14159265f);
        }

        const float delta = DirectX::XMScalarModAngle(m_heading - m_yaw);
        if (m_walking) m_yaw += (std::clamp)(delta, -TURN_SPEED * dt, TURN_SPEED * dt);
        const bool go = m_walking && std::fabs(delta) < 0.5f;
        const float speed = m_leaving ? WALK_SPEED * 1.3f : WALK_SPEED;
        body.velocity.x = go ? std::sin(m_yaw) * speed : 0.0f;
        body.velocity.z = go ? std::cos(m_yaw) * speed : 0.0f;
    }

    State m_state = State::IDLE;
    CaptureSequence m_sequence;
    float m_timer = 0.0f;
    DirectX::XMFLOAT2 m_home = {};  // punto de aparición (x, z)
    bool m_leaving = false;
    float m_age = 0.0f;
    float m_lifetime = LIFETIME_MIN;
    float m_appear = 0.0f;          // 0 = no se ve, 1 = del todo presente
    float m_yaw = 0.0f;
    float m_heading = 0.0f;   // hacia dónde quiere caminar
    float m_wanderTimer = 0.0f;
    bool m_walking = false;
    float m_hp = 1.0f;
    int m_speciesIndex = -1;
    int m_speciesId = -1;
    int m_level = 1;
    bool m_shiny = false;
    int m_ballId = -1;
};
