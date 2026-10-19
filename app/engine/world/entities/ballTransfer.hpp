#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../anim/easing.hpp"
#include "pokeball.hpp"

// Tiempos y alturas de sacar o guardar a un pokémon con su pokéball (todo en un sitio).
struct BallTransferConfig {
    float flyTime = 0.35f;      // la bola sale de la mano y vuela en arco hasta quedar suspendida
    float arcHeight = 0.7f;     // m extra a mitad del vuelo
    float openTime = 0.2f;      // se abre la tapa
    float beamTime = 0.5f;      // el rayo rojo saca (o mete) al pokémon
    float closeTime = 0.18f;    // se cierra en el aire
    float returnTime = 0.35f;   // vuelve cerrada hacia el jugador
    float shrinkAtEnd = 0.2f;   // fracción final del regreso en la que se encoge al llegar a la mano
    float beamWidth = 0.35f;    // ancho del rayo respecto al tamaño del pokémon
};
inline const BallTransferConfig kBallTransfer{};

// Sacar a un pokémon de su pokéball (RELEASE) o guardarlo en ella (ABSORB), siempre igual: la bola vuela desde la mano mirando en
// dirección contraria a la cámara, se abre en el aire, el rayo rojo saca o mete al pokémon, se cierra en el aire y vuelve cerrada
// hacia el jugador. Solo calcula la animación; quien la usa dibuja y reparte el pokémon.
class BallTransfer {
    public:
    enum class Mode { RELEASE, ABSORB };
    enum class Phase { NONE, FLY, OPEN, BEAM, CLOSE, RETURN };

    struct Pose { DirectX::XMFLOAT3 position = {}; float yaw = 0.0f, lid = 0.0f, scale = 1.0f, glow = 0.0f; };
    struct Beam { bool active = false; DirectX::XMFLOAT3 from = {}, to = {}; float width = 0.0f, intensity = 0.0f; };

    // 'yaw' hacia donde mira el botón; 'hand' desde donde sale.
    void start(Mode mode, float yaw, const DirectX::XMFLOAT3& hand, int ballType) {
        m_mode = mode;
        m_yaw = yaw;
        m_ballType = ballType;
        m_from = hand;
        m_hand = hand;
        m_hover = hand;
        m_phase = Phase::FLY;
        m_time = 0.0f;
    }
    void stop() { m_phase = Phase::NONE; }

    // 'hover': dónde queda suspendida; 'hand': dónde está ahora la mano (el regreso la sigue).
    void update(float dt, const DirectX::XMFLOAT3& hover, const DirectX::XMFLOAT3& hand) {
        if (m_phase == Phase::NONE) return;
        m_hover = hover;
        m_hand = hand;
        m_time += dt;
        const BallTransferConfig& c = kBallTransfer;
        switch (m_phase) {
            case Phase::FLY:    if (m_time >= c.flyTime) next(Phase::OPEN); break;
            case Phase::OPEN:   if (m_time >= c.openTime) next(Phase::BEAM); break;
            case Phase::BEAM:   if (m_time >= c.beamTime) next(Phase::CLOSE); break;
            case Phase::CLOSE:  if (m_time >= c.closeTime) next(Phase::RETURN); break;
            case Phase::RETURN: if (m_time >= c.returnTime) m_phase = Phase::NONE; break;
            default: break;
        }
    }

    bool active() const { return m_phase != Phase::NONE; }
    Phase phase() const { return m_phase; }
    int ballType() const { return m_ballType; }

    // 0 = el pokémon está dentro de la bola, 1 = fuera del todo.
    float emerge() const {
        const float u = Easing::apply(Easing::Curve::IN_OUT, Easing::remap(m_time, 0.0f, kBallTransfer.beamTime));
        const float out = m_mode == Mode::RELEASE ? u : 1.0f - u;
        switch (m_phase) {
            case Phase::FLY:
            case Phase::OPEN:   return m_mode == Mode::RELEASE ? 0.0f : 1.0f;
            case Phase::BEAM:   return out;
            default:            return m_mode == Mode::RELEASE ? 1.0f : 0.0f;
        }
    }

    Pose pose() const {
        const BallTransferConfig& c = kBallTransfer;
        Pose p;
        p.yaw = m_yaw;
        switch (m_phase) {
            case Phase::FLY: {
                const float u = Easing::apply(Easing::Curve::OUT_QUAD, Easing::remap(m_time, 0.0f, c.flyTime));
                p.position = mix(m_from, m_hover, u);
                p.position.y += std::sin(3.14159265f * u) * c.arcHeight;
                break;
            }
            case Phase::OPEN:   p.position = m_hover; p.lid = Easing::apply(Easing::Curve::OUT_CUBIC, Easing::remap(m_time, 0.0f, c.openTime)); break;
            case Phase::BEAM:   p.position = m_hover; p.lid = 1.0f; p.glow = 0.3f + 0.3f * Easing::pulse(Easing::remap(m_time, 0.0f, c.beamTime)); break;
            case Phase::CLOSE:  p.position = m_hover; p.lid = 1.0f - Easing::apply(Easing::Curve::IN_QUAD, Easing::remap(m_time, 0.0f, c.closeTime)); break;
            case Phase::RETURN: {
                const float u = Easing::remap(m_time, 0.0f, c.returnTime);
                p.position = mix(m_hover, m_hand, Easing::apply(Easing::Curve::IN_OUT, u));
                p.scale = 1.0f - Easing::apply(Easing::Curve::IN_QUAD, Easing::remap(u, 1.0f - c.shrinkAtEnd, 1.0f));
                break;
            }
            default: p.scale = 0.0f; break;
        }
        return p;
    }

    // Punto de la apertura (encima de la bola), de donde sale el rayo.
    DirectX::XMFLOAT3 opening() const {
        const DirectX::XMFLOAT3 b = pose().position;
        return { b.x, b.y + Pokeball::RADIUS * 0.8f, b.z };
    }

    // Rayo hacia el pokémon (centro 'target', tamaño 'size') mientras sale o entra.
    Beam beam(const DirectX::XMFLOAT3& target, float size) const {
        Beam b;
        if (m_phase != Phase::BEAM) return b;
        const float u = Easing::remap(m_time, 0.0f, kBallTransfer.beamTime);
        b.active = true;
        b.from = opening();
        b.to = target;
        b.width = (std::max)(0.05f, size * kBallTransfer.beamWidth);
        b.intensity = Easing::remap(u, 0.0f, 0.15f) * (1.0f - Easing::remap(u, 0.8f, 1.0f));
        return b;
    }

    // Posición del centro del pokémon mientras sale o entra: va de la apertura de la bola a su sitio.
    DirectX::XMFLOAT3 creatureCenter(const DirectX::XMFLOAT3& rest) const {
        return mix(opening(), rest, Easing::apply(Easing::Curve::OUT_QUAD, emerge()));
    }

    private:
    void next(Phase to) { m_phase = to; m_time = 0.0f; }
    static DirectX::XMFLOAT3 mix(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
    }

    Mode m_mode = Mode::RELEASE;
    Phase m_phase = Phase::NONE;
    float m_time = 0.0f, m_yaw = 0.0f;
    DirectX::XMFLOAT3 m_from = {}, m_hover = {}, m_hand = {};
    int m_ballType = -1;
};
