#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Pesca: lanzar el corcho al agua, esperar a que pique, recoger a tiempo y, con el pokémon enganchado, decidir si se captura o se suelta.
// Solo lleva la cuenta de las fases; qué pica y la captura las decide la escena.
class Fishing {
    public:
    static constexpr float CAST_MIN = 1.0f;       // alcance del lanzamiento (m): hay que estar junto al agua
    static constexpr float CAST_MAX = 5.5f;
    static constexpr float MIN_DEPTH = 1.0f;      // el corcho solo cae donde el agua tiene al menos esta profundidad
    static constexpr float CAST_TIME = 0.8f;      // lo que tarda el corcho en caer (la animación de lanzar)
    static constexpr float WAIT_MIN = 3.0f;       // espera hasta la picada (se divide por la velocidad de la caña)
    static constexpr float WAIT_MAX = 8.0f;
    static constexpr float BITE_WINDOW = 1.3f;    // tiempo para recoger cuando pica
    static constexpr float HOOK_TIME = 25.0f;     // con el pokémon enganchado y sin decidir, se suelta solo
    static constexpr float MAX_DISTANCE = 16.0f;  // más lejos que esto del corcho, se corta la línea

    enum class Phase { IDLE, CASTING, WAITING, BITING, HOOKED };
    enum class Event { NONE, BITE, MISSED, HOOKED, TIMEOUT, LINE_CUT };

    Phase phase() const { return m_phase; }
    bool active() const { return m_phase != Phase::IDLE; }
    bool hooked() const { return m_phase == Phase::HOOKED; }
    const DirectX::XMFLOAT3& bobber() const { return m_bobber; }
    float biteProgress() const { return m_phase == Phase::BITING ? m_timer / BITE_WINDOW : 0.0f; }

    // Punto de agua delante del jugador donde cae el corcho; false si no hay agua suficiente a su alcance.
    static bool findWater(const Physics::World& world, float x, float z, float yaw, DirectX::XMFLOAT3& out, float feet = Physics::World::ABOVE) {
        const float sx = std::sin(yaw), sz = std::cos(yaw);
        for (float d = CAST_MIN; d <= CAST_MAX; d += 0.5f) {
            const float px = x + sx * d, pz = z + sz * d;
            if (world.waterDepth(px, pz, feet) < MIN_DEPTH) continue;
            const float reach = (std::min)(d + 1.5f, CAST_MAX); // un poco más allá de la orilla
            const float fx = x + sx * reach, fz = z + sz * reach;
            const bool deep = world.waterDepth(fx, fz, feet) >= MIN_DEPTH;
            out = { deep ? fx : px, world.waterLevel(deep ? fx : px, deep ? fz : pz, feet), deep ? fz : pz };
            return true;
        }
        return false;
    }

    bool cast(const Physics::World& world, float x, float z, float yaw, float rodSpeed, float feet = Physics::World::ABOVE) {
        if (active() || !findWater(world, x, z, yaw, m_bobber, feet)) return false;
        m_feet = feet;
        m_phase = Phase::CASTING;
        m_timer = 0.0f;
        m_speed = (std::max)(rodSpeed, 0.1f);
        return true;
    }

    // Recoge la caña / suelta la línea: vuelve a empezar.
    void reset() { m_phase = Phase::IDLE; m_timer = 0.0f; }

    // 'reel' = el jugador pulsó recoger este frame. El corcho sube y baja con el agua.
    Event update(float dt, bool reel, const Physics::World& world, float playerX, float playerZ) {
        if (m_phase == Phase::IDLE) return Event::NONE;
        m_bobber.y = world.waterLevel(m_bobber.x, m_bobber.z, m_feet);
        const float dx = m_bobber.x - playerX, dz = m_bobber.z - playerZ;
        if (dx * dx + dz * dz > MAX_DISTANCE * MAX_DISTANCE) { reset(); return Event::LINE_CUT; }

        m_timer += dt;
        switch (m_phase) {
            case Phase::CASTING:
                if (m_timer >= CAST_TIME) {
                    m_phase = Phase::WAITING;
                    m_timer = 0.0f;
                    m_limit = RandomUtil::range(WAIT_MIN, WAIT_MAX) / m_speed;
                }
                break;
            case Phase::WAITING:
                if (reel) reset(); // la recoge sin nada
                else if (m_timer >= m_limit) {
                    m_phase = Phase::BITING;
                    m_timer = 0.0f;
                    return Event::BITE;
                }
                break;
            case Phase::BITING:
                if (reel) {
                    m_phase = Phase::HOOKED;
                    m_timer = 0.0f;
                    return Event::HOOKED;
                }
                if (m_timer >= BITE_WINDOW) { reset(); return Event::MISSED; }
                break;
            case Phase::HOOKED:
                if (m_timer >= HOOK_TIME) return Event::TIMEOUT;
                break;
            default: break;
        }
        return Event::NONE;
    }

    // Tras fallar la captura, el pokémon sigue enganchado: se reinicia la cuenta atrás.
    void renewHook() { m_timer = 0.0f; }
    // Segundos que quedan para decidir (con el pokémon enganchado).
    float hookRemaining() const { return m_phase == Phase::HOOKED ? (std::max)(0.0f, HOOK_TIME - m_timer) : 0.0f; }

    private:
    Phase m_phase = Phase::IDLE;
    DirectX::XMFLOAT3 m_bobber = {};
    float m_feet = Physics::World::ABOVE; // altura de los pies al lanzar (decide si el agua es la de una gruta)
    float m_timer = 0.0f;
    float m_limit = 0.0f;
    float m_speed = 1.0f;
};
