#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../anim/easing.hpp"
#include "../../anim/particles.hpp"
#include "../../anim/phaseMachine.hpp"
#include "../../physics/physicsWorld.hpp"
#include "../rules/captureRules.hpp"
#include "../entities/pokeball.hpp"

// Todos los tiempos y distancias de la animación de captura (nada de esto está repartido por el código).
struct CaptureAnimConfig {
    // 1. La bola choca con el pokémon y sale rodando hacia un lado (respecto a la cámara) girando.
    float deflectTime = 0.6f;
    float deflectDistance = 1.4f;    // m que se aparta
    float deflectMargin = 0.2f;      // holgura contra paredes y objetos
    float deflectHop = 2.4f;         // velocidad vertical inicial del saltito
    float tumbleTurns = 1.0f;        // vueltas sobre sí misma mientras se aparta
    // 2. Se abre la tapa.
    float openTime = 0.35f;
    // 3. El rayo absorbe al pokémon.
    float absorbTime = 0.95f;
    // 4. Se cierra mientras da un saltito, cae y gira hasta mirar a la cámara.
    float closeTime = 0.8f;
    float lidCloseTime = 0.3f;
    float closeHop = 3.4f;
    float turnSpeed = 7.0f;          // rad/s máximo al girar hacia la cámara
    float settleTimeout = 2.0f;
    // 5. Tambaleo de lado a lado.
    float leadPause = 0.45f;
    float wobbleTime = 0.8f;
    float wobblePause = 0.5f;
    float wobbleTilt = 0.55f;        // radianes
    // 6a. Captura: destello y estrellas; la bola se queda quieta y luego se desvanece.
    float flashTime = 0.25f;
    float starTime = 1.4f;
    float fadeTime = 0.4f;
    // 6b. Fallo: la bola se abre de golpe con destello y fragmentos y el pokémon vuelve a salir.
    float breakTime = 0.95f;
    float breakLidTime = 0.1f;
    float breakEmerge = 0.45f;       // el pokémon sale del rayo en este tiempo
    float breakShake = 0.12f;        // sacudida de la bola (rad)
};
inline const CaptureAnimConfig kCaptureAnim{};

// Animación de captura de un pokémon. El resultado ya está decidido (CaptureRules::Result); aquí solo se reproduce.
// Fases: DEFLECT (la bola se aparta girando) → OPEN (se abre la tapa) → ABSORB (el rayo rojo mete al pokémon) → CLOSE (se cierra, cae
// y gira hasta mirar a la cámara) → SETTLE → WOBBLE (tantos giros como indique el resultado) → CLICK (captura: estrellas) o BREAK
// (se escapa: la bola se abre de golpe y el pokémon sale de nuevo).
class CaptureSequence {
    public:
    enum class Phase { NONE, DEFLECT, OPEN, ABSORB, CLOSE, SETTLE, WOBBLE, CLICK, BREAK, DONE };

    // Cómo hay que dibujar la bola: posición, orientación (yaw, giro sobre sí misma y balanceo) y cuánto está abierta la tapa.
    struct BallPose {
        DirectX::XMFLOAT3 position = {};
        float yaw = 0.0f, spin = 0.0f, roll = 0.0f;
        float lid = 0.0f;   // 0 = cerrada, 1 = abierta del todo
        float scale = 1.0f;
        float glow = 0.0f;
    };
    // Silueta roja aditiva del pokémon mientras lo absorbe el rayo (o sale de él).
    struct Silhouette { bool active = false; DirectX::XMFLOAT3 center = {}; float size = 0.0f; float intensity = 0.0f; };
    // Rayo entre la apertura de la bola y la silueta.
    struct Beam { bool active = false; DirectX::XMFLOAT3 from = {}, to = {}; float width = 0.0f; float intensity = 0.0f; };
    struct Flash { bool active = false; DirectX::XMFLOAT3 center = {}, color = {}; float radius = 0.0f, intensity = 0.0f; };

    struct Subject { DirectX::XMFLOAT3 center = {}; float size = 0.5f; }; // el pokémon capturado

    void start(const Pokeball& ball, const CaptureRules::Result& result, const Subject& subject, float cameraYaw, const Physics::World& world) {
        m_ball = ball;
        m_ball.body.restitution = 0.0f;
        m_ball.body.rolls = false;
        m_ball.body.groundDrag = 0.0f;
        m_ball.body.velocity = { 0.0f, 0.0f, 0.0f };
        m_result = result;
        m_subject = subject;
        m_cameraYaw = cameraYaw;
        m_particles.clear();
        m_flashTime = m_flashDuration = 0.0f;
        m_startYaw = m_yaw = m_ball.yaw;
        m_spin = 0.0f;
        chooseDeflection(world);
        m_machine.reset(Phase::DEFLECT);
        m_ball.body.velocity.y = kCaptureAnim.deflectHop;
        m_ball.body.onGround = false;
    }

    // La cámara puede girar durante la animación: la bola acaba mirándola.
    void watch(float cameraYaw) { m_cameraYaw = cameraYaw; }

    // Avanza la secuencia. Devuelve true en el instante en que se revela el resultado (clic o fallo).
    bool update(float dt, const Physics::World& world) {
        m_machine.tick(dt);
        m_particles.update(dt);
        m_flashTime = (std::max)(0.0f, m_flashTime - dt);
        steer(dt);
        world.step(m_ball.body, dt);

        bool reveal = false;
        advance(world, reveal);
        m_machine.dispatch([this](Phase, Phase to) { onEnter(to); });
        return reveal;
    }

    bool finished() const { return phase() == Phase::DONE; }
    bool captured() const { return m_result.captured; }
    CaptureRules::Throw kind() const { return m_result.kind; }
    Phase phase() const { return m_machine.phase(); }
    const Pokeball& ball() const { return m_ball; }
    const Anim::ParticleSystem& particles() const { return m_particles; }

    // --- Pose del pokémon: escala (0..1) ---
    float pokemonScale() const {
        const CaptureAnimConfig& c = kCaptureAnim;
        switch (phase()) {
            case Phase::DEFLECT:
            case Phase::OPEN:   return 1.0f;
            case Phase::ABSORB: return 1.0f - Easing::apply(Easing::Curve::OUT_QUAD, Easing::remap(m_machine.time(), 0.0f, c.absorbTime * 0.12f));
            case Phase::BREAK:  return Easing::apply(Easing::Curve::OUT_QUAD, Easing::remap(m_machine.time(), c.breakEmerge * 0.55f, c.breakEmerge));
            default:            return 0.0f;
        }
    }

    BallPose ballPose() const {
        const CaptureAnimConfig& c = kCaptureAnim;
        BallPose pose;
        pose.position = m_ball.body.position;
        pose.yaw = m_yaw;
        pose.spin = m_spin;
        pose.roll = tilt();
        pose.lid = lid();
        const float t = m_machine.time();
        switch (phase()) {
            case Phase::ABSORB: pose.glow = 0.35f + 0.25f * Easing::pulse(Easing::remap(t, 0.0f, c.absorbTime)); break;
            case Phase::CLICK: {
                pose.glow = 1.0f - m_machine.progress(c.flashTime);
                pose.scale = 1.0f - Easing::apply(Easing::Curve::IN_OUT, Easing::remap(t, c.flashTime + c.starTime, c.flashTime + c.starTime + c.fadeTime));
                break;
            }
            case Phase::BREAK: {
                pose.roll += c.breakShake * std::sin(t * 70.0f) * (1.0f - Easing::remap(t, 0.0f, c.breakTime * 0.6f));
                pose.glow = 0.7f * (1.0f - Easing::remap(t, 0.0f, c.breakTime * 0.5f));
                pose.scale = 1.0f - Easing::apply(Easing::Curve::IN_QUAD, Easing::remap(t, c.breakTime * 0.65f, c.breakTime));
                break;
            }
            case Phase::DONE: pose.scale = 0.0f; break;
            default: break;
        }
        return pose;
    }

    Silhouette silhouette() const {
        const CaptureAnimConfig& c = kCaptureAnim;
        Silhouette s;
        const float t = m_machine.time();
        if (phase() == Phase::ABSORB) {
            const float u = m_machine.progress(c.absorbTime);
            const float move = Easing::apply(Easing::Curve::IN_OUT, Easing::remap(u, 0.08f, 1.0f));
            s.active = true;
            s.center = mix(m_subject.center, opening(), move);
            s.size = m_subject.size * (1.0f - Easing::apply(Easing::Curve::IN_CUBIC, Easing::remap(u, 0.25f, 1.0f)));
            s.intensity = 0.55f + 0.45f * Easing::pulse(Easing::remap(u, 0.0f, 0.5f));
        } else if (phase() == Phase::BREAK && t < c.breakEmerge) {
            const float u = Easing::remap(t, 0.0f, c.breakEmerge);
            s.active = true;
            s.center = mix(opening(), m_subject.center, Easing::apply(Easing::Curve::OUT_CUBIC, u));
            s.size = m_subject.size * Easing::apply(Easing::Curve::OUT_QUAD, u) * (1.0f - Easing::remap(u, 0.6f, 1.0f));
            s.intensity = 1.0f - 0.4f * u;
        }
        return s;
    }

    Beam beam() const {
        const CaptureAnimConfig& c = kCaptureAnim;
        Beam b;
        if (phase() != Phase::ABSORB) return b;
        const float u = m_machine.progress(c.absorbTime);
        const Silhouette s = silhouette();
        b.active = true;
        b.from = opening();
        b.to = s.center;
        b.width = (std::max)(0.05f, m_subject.size * 0.55f * (1.0f - 0.6f * u));
        b.intensity = Easing::remap(u, 0.0f, 0.12f) * (1.0f - Easing::remap(u, 0.85f, 1.0f));
        return b;
    }

    Flash flash() const {
        Flash f;
        if (m_flashTime <= 0.0f || m_flashDuration <= 0.0f) return f;
        const float u = 1.0f - m_flashTime / m_flashDuration;
        f.active = true;
        f.center = m_flashPosition;
        f.color = m_flashColor;
        f.radius = m_flashRadius * (0.4f + 0.9f * Easing::apply(Easing::Curve::OUT_CUBIC, u));
        f.intensity = 1.0f - u;
        return f;
    }

    // Punto de la apertura de la bola (de donde sale el rayo): encima, algo hacia delante.
    DirectX::XMFLOAT3 opening() const {
        const DirectX::XMFLOAT3& b = m_ball.body.position;
        const float r = Pokeball::RADIUS;
        return { b.x + std::sin(m_yaw) * r * 0.35f, b.y + r * 0.8f, b.z + std::cos(m_yaw) * r * 0.35f };
    }

    private:
    // --- Dirección en la que se aparta la bola ---
    // Lado de la cámara con más sitio libre (paredes, objetos, desniveles) y que no la meta dentro del pokémon.
    float clearance(const DirectX::XMFLOAT3& dir, const Physics::World& world) const {
        const CaptureAnimConfig& c = kCaptureAnim;
        const DirectX::XMFLOAT3& p = m_ball.body.position;
        const float reach = c.deflectDistance + c.deflectMargin;
        float free = reach;
        for (float step = 0.1f; step <= reach; step += 0.1f) { // primer punto con muro, objeto, desnivel brusco o agua
            const DirectX::XMFLOAT3 at = { p.x + dir.x * step, p.y, p.z + dir.z * step };
            const bool wall = world.blocked(p, at) || world.groundHeight(at.x, at.z) > p.y + 0.35f;
            if (wall || world.waterDepth(at.x, at.z) > Physics::World::SWIM_DEPTH) { free = (std::max)(0.0f, step - 0.1f); break; }
        }
        return free;
    }

    void chooseDeflection(const Physics::World& world) {
        const CaptureAnimConfig& c = kCaptureAnim;
        const DirectX::XMFLOAT3 right = { std::cos(m_cameraYaw), 0.0f, -std::sin(m_cameraYaw) };
        const DirectX::XMFLOAT3& p = m_ball.body.position;
        float nx = p.x - m_subject.center.x, nz = p.z - m_subject.center.z; // de dónde viene el golpe respecto al pokémon
        const float nl = std::sqrt(nx * nx + nz * nz);
        if (nl > 1.0e-3f) { nx /= nl; nz /= nl; }
        float bestScore = -1.0e9f;
        for (const float sign : { 1.0f, -1.0f }) {
            const DirectX::XMFLOAT3 dir = { right.x * sign, 0.0f, right.z * sign };
            const float away = dir.x * nx + dir.z * nz; // >0: se aleja del pokémon
            const float free = clearance(dir, world);
            const float score = free + 0.9f * away + (sign > 0.0f ? 0.05f : 0.0f);
            if (score > bestScore) {
                bestScore = score;
                m_side = dir;
                m_distance = (std::min)(c.deflectDistance, (std::max)(0.0f, free - c.deflectMargin));
            }
        }
    }

    float yawToSubject() const {
        const DirectX::XMFLOAT3& b = m_ball.body.position;
        return std::atan2(m_subject.center.x - b.x, m_subject.center.z - b.z);
    }
    float yawToCamera() const { return m_cameraYaw + 3.14159265f; } // el botón hacia la cámara

    // Velocidad y orientación de la bola en cada fase (antes de integrar la física).
    void steer(float dt) {
        const CaptureAnimConfig& c = kCaptureAnim;
        const float t = m_machine.time();
        m_ball.body.velocity.x = m_ball.body.velocity.z = 0.0f;
        switch (phase()) {
            case Phase::DEFLECT: {
                const float u = (std::min)(t / c.deflectTime, 1.0f);
                const float speed = 2.0f * m_distance * (1.0f - u) / c.deflectTime; // derivada de un desplazamiento ease-out
                m_ball.body.velocity.x = m_side.x * speed;
                m_ball.body.velocity.z = m_side.z * speed;
                const float ease = Easing::apply(Easing::Curve::OUT_QUAD, u);
                m_yaw = Easing::lerpAngle(m_startYaw, yawToSubject(), ease);
                m_spin = 6.2831853f * c.tumbleTurns * ease;
                break;
            }
            case Phase::OPEN:
            case Phase::ABSORB:
                m_yaw = Easing::approachAngle(m_yaw, yawToSubject(), c.turnSpeed * dt);
                m_spin = 0.0f;
                break;
            case Phase::CLOSE:
            case Phase::SETTLE:
                m_yaw = Easing::approachAngle(m_yaw, yawToCamera(), c.turnSpeed * dt);
                break;
            case Phase::WOBBLE:
            case Phase::CLICK:
                m_yaw = Easing::approachAngle(m_yaw, yawToCamera(), c.turnSpeed * dt);
                break;
            default:
                break;
        }
    }

    void advance(const Physics::World&, bool& reveal) {
        const CaptureAnimConfig& c = kCaptureAnim;
        switch (phase()) {
            case Phase::DEFLECT: if (m_machine.elapsed(c.deflectTime)) m_machine.enter(Phase::OPEN); break;
            case Phase::OPEN:    if (m_machine.elapsed(c.openTime)) m_machine.enter(Phase::ABSORB); break;
            case Phase::ABSORB:  if (m_machine.elapsed(c.absorbTime)) m_machine.enter(Phase::CLOSE); break;
            case Phase::CLOSE:   if (m_machine.elapsed(c.closeTime)) m_machine.enter(Phase::SETTLE); break;
            case Phase::SETTLE:  if (m_ball.body.onGround || m_machine.elapsed(c.settleTimeout)) m_machine.enter(Phase::WOBBLE); break;
            case Phase::WOBBLE:
                if (m_machine.elapsed(wobbleDuration())) {
                    m_machine.enter(m_result.captured ? Phase::CLICK : Phase::BREAK);
                    reveal = true;
                }
                break;
            case Phase::CLICK:   if (m_machine.elapsed(c.flashTime + c.starTime + c.fadeTime)) m_machine.enter(Phase::DONE); break;
            case Phase::BREAK:   if (m_machine.elapsed(c.breakTime)) m_machine.enter(Phase::DONE); break;
            default: break;
        }
    }

    // Lo que ocurre al entrar en una fase: impulsos, destellos y ráfagas de partículas.
    void onEnter(Phase to) {
        const CaptureAnimConfig& c = kCaptureAnim;
        switch (to) {
            case Phase::ABSORB:
                m_spin = 0.0f;
                startFlash(opening(), { 1.0f, 0.25f, 0.2f }, 0.5f, 0.3f);
                break;
            case Phase::CLOSE:
                m_ball.body.velocity.y = c.closeHop;
                m_ball.body.onGround = false;
                startFlash(opening(), { 1.0f, 0.5f, 0.4f }, 0.45f, 0.25f);
                break;
            case Phase::CLICK:
                startFlash(m_ball.body.position, starColor(), 0.9f, c.flashTime);
                burstStars();
                break;
            case Phase::BREAK:
                startFlash(opening(), { 1.0f, 0.55f, 0.45f }, 1.4f, 0.4f);
                burstFragments();
                break;
            default:
                break;
        }
    }

    void startFlash(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& color, float radius, float duration) {
        m_flashPosition = at;
        m_flashColor = color;
        m_flashRadius = radius;
        m_flashTime = m_flashDuration = duration;
    }

    DirectX::XMFLOAT3 starColor() const {
        switch (m_result.kind) {
            case CaptureRules::Throw::SUPER_LUCKY: return { 0.70f, 0.95f, 1.00f };
            case CaptureRules::Throw::LUCKY:       return { 1.00f, 0.85f, 0.25f };
            default:                               return { 1.00f, 0.92f, 0.30f };
        }
    }

    // Estrellas: más y más grandes cuanto más especial es el lanzamiento.
    void burstStars() {
        const CaptureAnimConfig& c = kCaptureAnim;
        Anim::Burst burst;
        const bool super = m_result.kind == CaptureRules::Throw::SUPER_LUCKY, lucky = m_result.kind == CaptureRules::Throw::LUCKY;
        burst.count = super ? 16 : lucky ? 12 : 8;
        burst.lifeMin = c.starTime * 0.8f;
        burst.lifeMax = c.starTime;
        burst.speedMin = 1.4f;
        burst.speedMax = 2.6f;
        burst.spread = 1.25f;
        burst.gravity = 5.0f;
        burst.sizeStart = super ? 0.30f : lucky ? 0.22f : 0.16f;
        burst.sizeEnd = 0.0f;
        burst.colorStart = burst.colorEnd = starColor();
        burst.spin = 7.0f;
        burst.shape = Anim::Shape::STAR;
        const DirectX::XMFLOAT3& b = m_ball.body.position;
        m_particles.burst(burst, { b.x, b.y + 0.25f, b.z });
    }

    // Fragmentos de la bola (sin romper su malla): cubitos rojos y blancos que salen despedidos.
    void burstFragments() {
        Anim::Burst burst;
        burst.count = 14;
        burst.lifeMin = 0.5f;
        burst.lifeMax = 0.9f;
        burst.speedMin = 2.0f;
        burst.speedMax = 4.2f;
        burst.spread = 1.4f;
        burst.gravity = 12.0f;
        burst.sizeStart = 0.07f;
        burst.sizeEnd = 0.02f;
        burst.colorStart = { 0.9f, 0.18f, 0.16f };
        burst.colorEnd = { 0.95f, 0.95f, 0.95f };
        burst.spin = 14.0f;
        burst.shape = Anim::Shape::CUBE;
        m_particles.burst(burst, opening());
        burst.count = 8;
        burst.colorStart = { 1.0f, 1.0f, 0.85f };
        burst.sizeStart = 0.05f;
        burst.speedMin = 3.0f;
        burst.speedMax = 5.5f;
        m_particles.burst(burst, opening());
    }

    // --- Pose de la bola ---
    float lid() const {
        const CaptureAnimConfig& c = kCaptureAnim;
        switch (phase()) {
            case Phase::OPEN:   return m_machine.progress(c.openTime, Easing::Curve::OUT_CUBIC);
            case Phase::ABSORB: return 1.0f;
            case Phase::CLOSE:  return 1.0f - m_machine.progress(c.lidCloseTime, Easing::Curve::IN_QUAD);
            case Phase::BREAK:  return Easing::apply(Easing::Curve::OUT_BACK, Easing::remap(m_machine.time(), 0.0f, c.breakLidTime));
            default:            return 0.0f;
        }
    }

    // Balanceo de lado a lado (giro sobre el eje del botón).
    float tilt() const {
        const CaptureAnimConfig& c = kCaptureAnim;
        if (phase() != Phase::WOBBLE) return 0.0f;
        const float t = m_machine.time() - c.leadPause;
        if (t < 0.0f) return 0.0f;

        const float period = c.wobbleTime + c.wobblePause;
        const int index = static_cast<int>(t / period);
        const float local = t - static_cast<float>(index) * period;
        if (index >= m_result.wobbles || local >= c.wobbleTime) return 0.0f;
        return c.wobbleTilt * std::sin(6.2831853f * local / c.wobbleTime);
    }

    float wobbleDuration() const {
        const CaptureAnimConfig& c = kCaptureAnim;
        return c.leadPause + static_cast<float>(m_result.wobbles) * (c.wobbleTime + c.wobblePause);
    }

    static DirectX::XMFLOAT3 mix(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
    }

    Pokeball m_ball;
    CaptureRules::Result m_result = { false, 0, CaptureRules::Throw::NORMAL };
    Subject m_subject;
    PhaseMachine<Phase> m_machine;
    Anim::ParticleSystem m_particles;
    DirectX::XMFLOAT3 m_side = { 1.0f, 0.0f, 0.0f };
    float m_distance = 0.0f;
    float m_cameraYaw = 0.0f;
    float m_yaw = 0.0f, m_startYaw = 0.0f, m_spin = 0.0f;
    DirectX::XMFLOAT3 m_flashPosition = {}, m_flashColor = { 1.0f, 1.0f, 1.0f };
    float m_flashRadius = 0.5f, m_flashTime = 0.0f, m_flashDuration = 0.0f;
};
