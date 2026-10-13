#pragma once
#include <algorithm>
#include "easing.hpp"

// Máquina de estados mínima para animaciones por fases: guarda la fase actual y el tiempo que lleva en ella, y avisa de cada
// transición (de, a) a quien llame a dispatch(). Es copiable (no guarda funciones), así que puede vivir dentro de cualquier objeto.
template <typename Phase>
class PhaseMachine {
    public:
    Phase phase() const { return m_phase; }
    float time() const { return m_time; }
    bool in(Phase phase) const { return m_phase == phase; }

    void reset(Phase phase) {
        m_phase = phase;
        m_time = 0.0f;
        m_events = 0;
    }

    // Cambia de fase y deja constancia de la transición.
    void enter(Phase to) {
        if (m_events < MAX_EVENTS) m_from[m_events++] = m_phase;
        m_phase = to;
        m_time = 0.0f;
    }

    void tick(float dt) { m_time += dt; }

    // Avance (0..1) por la fase actual con una duración y una curva.
    float progress(float duration, Easing::Curve curve = Easing::Curve::LINEAR) const {
        return Easing::apply(curve, duration > 0.0f ? (std::min)(m_time / duration, 1.0f) : 1.0f);
    }
    bool elapsed(float duration) const { return m_time >= duration; }

    // Llama a f(from, to) por cada transición hecha desde la última vez. Dentro de f se puede volver a cambiar de fase.
    template <typename F>
    void dispatch(F&& f) {
        for (int guard = 0; m_events > 0 && guard < MAX_EVENTS * 2; ++guard) {
            const Phase from = m_from[0];
            for (int i = 1; i < m_events; ++i) m_from[i - 1] = m_from[i];
            --m_events;
            f(from, m_phase);
        }
    }

    private:
    static constexpr int MAX_EVENTS = 4;
    Phase m_phase{};
    float m_time = 0.0f;
    Phase m_from[MAX_EVENTS] = {};
    int m_events = 0;
};
