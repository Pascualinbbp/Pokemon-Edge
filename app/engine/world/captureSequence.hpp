#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"
#include "captureRules.hpp"
#include "pokeball.hpp"

// Animación de captura de un pokémon. El resultado ya está decidido (CaptureRules::Result); aquí solo se reproduce:
// el pokémon se encoge dentro de la bola, la bola cae al suelo con la misma física que el resto, se tambalea tantas
// veces como indique el resultado y termina con estrellas (captura) o abriéndose (fallo).
class CaptureSequence {
    public:
    enum class Phase { NONE, ABSORB, SETTLE, WOBBLE, CLICK, BREAK, DONE };

    static constexpr float ABSORB_TIME = 0.5f;     // el pokémon entra en la bola
    static constexpr float SETTLE_TIMEOUT = 2.0f;  // espera máxima a que la bola toque el suelo
    static constexpr float LEAD_PAUSE = 0.45f;     // pausa antes del primer giro
    static constexpr float WOBBLE_TIME = 0.8f;     // duración de un giro
    static constexpr float WOBBLE_PAUSE = 0.5f;    // pausa entre giros
    static constexpr float WOBBLE_TILT = 0.55f;    // inclinación máxima (radianes)
    static constexpr float FLASH_TIME = 0.25f;     // destello al capturar
    static constexpr float STAR_TIME = 1.4f;       // duración de las estrellas
    static constexpr float FADE_TIME = 0.4f;       // la bola desaparece tras las estrellas
    static constexpr float BREAK_TIME = 0.45f;     // la bola se abre y el pokémon sale
    static constexpr float STAR_SPEED = 1.8f;
    static constexpr float STAR_LIFT = 2.6f;
    static constexpr float STAR_GRAVITY = 5.0f;

    void start(const Pokeball& ball, const CaptureRules::Result& result) {
        m_ball = ball;
        m_ball.body.velocity = { 0.0f, 0.0f, 0.0f }; // cae en vertical junto al pokémon
        m_ball.body.onGround = false;
        m_result = result;
        enter(Phase::ABSORB);
    }

    // Avanza la secuencia. Devuelve true en el instante en que se revela el resultado (clic o fallo).
    bool update(float dt, const Physics::World& world) {
        m_time += dt;
        world.step(m_ball.body, dt);

        switch (m_phase) {
            case Phase::ABSORB:
                if (m_time >= ABSORB_TIME) enter(Phase::SETTLE);
                break;
            case Phase::SETTLE:
                if (m_ball.body.onGround || m_time >= SETTLE_TIMEOUT) enter(Phase::WOBBLE);
                break;
            case Phase::WOBBLE:
                if (m_time >= wobbleDuration()) {
                    enter(m_result.captured ? Phase::CLICK : Phase::BREAK);
                    return true;
                }
                break;
            case Phase::CLICK:
                if (m_time >= FLASH_TIME + STAR_TIME + FADE_TIME) enter(Phase::DONE);
                break;
            case Phase::BREAK:
                if (m_time >= BREAK_TIME) enter(Phase::DONE);
                break;
            default:
                break;
        }
        return false;
    }

    bool finished() const { return m_phase == Phase::DONE; }
    bool captured() const { return m_result.captured; }
    CaptureRules::Throw kind() const { return m_result.kind; }
    const Pokeball& ball() const { return m_ball; }

    // --- Pose del pokémon: escala (0..1) y cuánto se acerca a la bola (0 = en su sitio, 1 = dentro de la bola) ---
    float pokemonScale() const {
        if (m_phase == Phase::ABSORB) return 1.0f - ease(progress(ABSORB_TIME));
        if (m_phase == Phase::BREAK) return ease(progress(BREAK_TIME));
        return 0.0f;
    }

    float pokemonBlend() const {
        if (m_phase == Phase::ABSORB) return ease(progress(ABSORB_TIME));
        if (m_phase == Phase::BREAK) return 1.0f - ease(progress(BREAK_TIME));
        return 1.0f;
    }

    // --- Pose de la bola ---
    float ballTilt() const {
        if (m_phase != Phase::WOBBLE) return 0.0f;
        const float t = m_time - LEAD_PAUSE;
        if (t < 0.0f) return 0.0f;

        const float period = WOBBLE_TIME + WOBBLE_PAUSE;
        const int index = static_cast<int>(t / period);
        const float local = t - static_cast<float>(index) * period;
        if (index >= m_result.wobbles || local >= WOBBLE_TIME) return 0.0f;
        return WOBBLE_TILT * std::sin(6.2831853f * local / WOBBLE_TIME);
    }

    float ballScale() const {
        switch (m_phase) {
            case Phase::CLICK:
                return 1.0f - ease(std::clamp((m_time - FLASH_TIME - STAR_TIME) / FADE_TIME, 0.0f, 1.0f));
            case Phase::BREAK: {
                const float u = progress(BREAK_TIME);
                return (1.0f + 0.35f * std::sin(3.1415927f * u)) * (1.0f - ease(u));
            }
            case Phase::DONE:
                return 0.0f;
            default:
                return 1.0f;
        }
    }

    // Brillo de la bola (0 = iluminada con normalidad, 1 = a pleno color).
    float ballGlow() const {
        switch (m_phase) {
            case Phase::ABSORB: return 1.0f - progress(ABSORB_TIME);
            case Phase::CLICK:  return 1.0f - progress(FLASH_TIME);
            case Phase::BREAK:  return std::sin(3.1415927f * progress(BREAK_TIME));
            default:            return 0.0f;
        }
    }

    // --- Estrellas (solo al capturar): más y más grandes cuanto más especial es el lanzamiento ---
    int starCount() const {
        if (m_phase != Phase::CLICK) return 0;
        switch (m_result.kind) {
            case CaptureRules::Throw::SUPER_LUCKY: return 16;
            case CaptureRules::Throw::LUCKY:       return 12;
            default:                               return 8;
        }
    }

    DirectX::XMFLOAT3 starColor() const {
        switch (m_result.kind) {
            case CaptureRules::Throw::SUPER_LUCKY: return { 0.70f, 0.95f, 1.00f };
            case CaptureRules::Throw::LUCKY:       return { 1.00f, 0.85f, 0.25f };
            default:                               return { 1.00f, 0.92f, 0.30f };
        }
    }

    void star(int index, DirectX::XMFLOAT3& position, float& scale, float& spin) const {
        const float t = (std::min)(m_time, STAR_TIME);
        const float angle = 6.2831853f * static_cast<float>(index) / static_cast<float>(starCount()) + 0.5f;
        const float reach = STAR_SPEED * ((index & 1) ? 1.0f : 0.65f) * t;
        const DirectX::XMFLOAT3& b = m_ball.body.position;
        position = { b.x + std::cos(angle) * reach, b.y + 0.25f + STAR_LIFT * t - 0.5f * STAR_GRAVITY * t * t, b.z + std::sin(angle) * reach };

        const float size = m_result.kind == CaptureRules::Throw::SUPER_LUCKY ? 0.30f : m_result.kind == CaptureRules::Throw::LUCKY ? 0.22f : 0.16f;
        scale = size * (std::min)(1.0f, t * 10.0f) * (1.0f - ease(t / STAR_TIME));
        spin = t * 7.0f + static_cast<float>(index);
    }

    private:
    void enter(Phase phase) {
        m_phase = phase;
        m_time = 0.0f;
    }

    float wobbleDuration() const {
        return LEAD_PAUSE + static_cast<float>(m_result.wobbles) * (WOBBLE_TIME + WOBBLE_PAUSE);
    }

    float progress(float duration) const { return (std::min)(m_time / duration, 1.0f); }
    static float ease(float x) { return x * x * (3.0f - 2.0f * x); }

    Pokeball m_ball;
    CaptureRules::Result m_result = { false, 0, CaptureRules::Throw::NORMAL };
    Phase m_phase = Phase::NONE;
    float m_time = 0.0f;
};
