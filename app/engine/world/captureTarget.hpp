#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <DirectXMath.h>
#include "../physics/body.hpp"

// Objetivo de pruebas: un cubo inmóvil que hace de pokémon. Al recibir una Pokéball se "captura"
// (se encoge y desaparece) y tras unos segundos reaparece en otro sitio para seguir probando.
class CaptureTarget {
    public:
    static constexpr float SIZE          = 1.2f;  // arista del cubo
    static constexpr float HALF          = SIZE * 0.5f;
    static constexpr float SHADOW_RADIUS = 0.9f;
    static constexpr float CAPTURE_TIME  = 0.5f;  // duración de la animación de captura
    static constexpr float RESPAWN_DELAY = 2.5f;  // segundos oculto antes de reaparecer

    Physics::Body body; // inmóvil: nunca se integra con la física

    CaptureTarget() { moveTo(0); }

    bool hittable() const { return m_state == State::IDLE; }
    bool visible() const { return m_state != State::HIDDEN; }

    // Arista actual del cubo dibujado (se encoge al capturarlo).
    float scale() const {
        switch (m_state) {
            case State::IDLE:      return SIZE;
            case State::CAPTURING: return SIZE * (std::max)(0.0f, 1.0f - m_timer / CAPTURE_TIME);
            default:               return 0.0f;
        }
    }

    DirectX::XMFLOAT3 center() const { return { body.position.x, body.position.y + HALF, body.position.z }; }

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

    void capture() {
        m_state = State::CAPTURING;
        m_timer = 0.0f;
        body.shadowRadius = 0.0f;
    }

    void update(float dt) {
        switch (m_state) {
            case State::IDLE:
                break;
            case State::CAPTURING:
                m_timer += dt;
                if (m_timer >= CAPTURE_TIME) {
                    m_state = State::HIDDEN;
                    m_timer = 0.0f;
                }
                break;
            case State::HIDDEN:
                m_timer += dt;
                if (m_timer >= RESPAWN_DELAY) moveTo(m_spawnIndex + 1);
                break;
        }
    }

    private:
    enum class State { IDLE, CAPTURING, HIDDEN };

    // Posiciones (x, z) por las que va apareciendo el cubo.
    static constexpr float SPAWNS[][2] = { { 0.0f, 14.0f }, { -12.0f, 18.0f }, { 14.0f, 10.0f }, { -8.0f, -14.0f }, { 10.0f, -12.0f } };
    static constexpr int SPAWN_COUNT = static_cast<int>(sizeof(SPAWNS) / sizeof(SPAWNS[0]));

    void moveTo(int index) {
        m_spawnIndex = index % SPAWN_COUNT;
        body.position = { SPAWNS[m_spawnIndex][0], 0.0f, SPAWNS[m_spawnIndex][1] };
        body.shadowRadius = SHADOW_RADIUS;
        m_state = State::IDLE;
        m_timer = 0.0f;
    }

    State m_state = State::IDLE;
    float m_timer = 0.0f;
    int m_spawnIndex = 0;
};
