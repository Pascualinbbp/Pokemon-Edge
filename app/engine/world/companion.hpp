#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../physics/physicsWorld.hpp"

// El pokémon líder del equipo, que acompaña al jugador por el mundo (física común, como cualquier otro cuerpo).
// Sigue al jugador por detrás y, si pasa cerca de un recurso que sabe trabajar, lo trabaja solo.
class Companion {
    public:
    static constexpr float SIZE = 0.7f;
    static constexpr float FOLLOW_BEHIND = 2.2f;  // distancia detrás del jugador
    static constexpr float FOLLOW_SIDE = 1.1f;    // desplazamiento a la derecha
    static constexpr float SPEED = 8.0f;
    static constexpr float TELEPORT_DISTANCE = 20.0f; // si se queda más lejos, aparece junto al jugador
    static constexpr float JUMP_SPEED = 8.0f;
    static constexpr float WORK_RANGE = 3.0f;     // distancia al borde de un recurso para trabajarlo
    static constexpr float WORK_INTERVAL = 1.4f;  // segundos por golpe con power 1
    static constexpr float LEVEL_SPEEDUP = 0.25f; // cada nivel de recolección por encima del 1 trabaja un 25 % más rápido
    static constexpr float HOVER_HEIGHT = 1.3f;   // altura sobre el jugador al levitar

    Physics::Body body;

    Companion() {
        body.collisionRadius = SIZE * 0.5f;
        body.collisionHeight = SIZE;
    }

    bool active() const { return m_speciesId >= 0; }
    int speciesId() const { return m_speciesId; }
    float yaw() const { return m_yaw; }
    bool working() const { return m_working; }

    // Velocidad de trabajo de un pokémon según su nivel de recolección y la ayuda del jugador.
    static float workPower(int level, float teamBonus) { return (1.0f + LEVEL_SPEEDUP * static_cast<float>(level - 1)) * teamBonus; }
    float bob() const { return m_working ? std::sin(m_time * 14.0f) * 0.07f : 0.0f; }

    // Cambia de pokémon (-1 = ninguno): aparece junto al jugador.
    void set(int speciesId, bool floats, const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        m_speciesId = speciesId;
        m_floats = floats;
        body.gravityScale = floats ? 0.0f : 1.0f;
        m_workTimer = 0.0f;
        m_working = false;
        const DirectX::XMFLOAT2 spot = followSpot(playerPosition, cameraYaw);
        body.position = { spot.x, playerPosition.y, spot.y };
        body.velocity = { 0.0f, 0.0f, 0.0f };
        body.onGround = false;
    }

    // Avanza el seguimiento. 'power' > 0 = trabajando este frame (el multiplicador del pokémon); devuelve true cuando
    // completa un golpe.
    bool update(float dt, const Physics::World& world, const DirectX::XMFLOAT3& playerPosition, float cameraYaw, float power) {
        if (!active()) return false;
        m_time += dt;

        const DirectX::XMFLOAT2 spot = followSpot(playerPosition, cameraYaw);
        const float dx = spot.x - body.position.x, dz = spot.y - body.position.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance > TELEPORT_DISTANCE) {
            body.position = { spot.x, playerPosition.y, spot.y };
            body.velocity = { 0.0f, 0.0f, 0.0f };
            body.onGround = false;
        } else if (distance > 0.3f) {
            const float speed = (std::min)(SPEED, distance * 3.0f);
            body.velocity.x = dx / distance * speed;
            body.velocity.z = dz / distance * speed;
            m_yaw = std::atan2(dx, dz);
            // Si el jugador está más alto (sobre una roca...), salta tras él.
            if (!m_floats && body.onGround && playerPosition.y - body.position.y > 0.3f && distance < 3.0f) world.jump(body, JUMP_SPEED);
        } else {
            body.velocity.x = body.velocity.z = 0.0f;
        }
        if (m_floats) body.velocity.y = (playerPosition.y + HOVER_HEIGHT - body.position.y) * 6.0f;
        world.step(body, dt);

        m_working = power > 0.0f;
        if (!m_working) {
            m_workTimer = 0.0f;
            return false;
        }
        m_workTimer += dt * power;
        if (m_workTimer < WORK_INTERVAL) return false;
        m_workTimer = 0.0f;
        return true;
    }

    private:
    // Punto donde se coloca: detrás y a la derecha del jugador respecto a la cámara.
    static DirectX::XMFLOAT2 followSpot(const DirectX::XMFLOAT3& player, float cameraYaw) {
        const float s = std::sin(cameraYaw), c = std::cos(cameraYaw);
        return { player.x - s * FOLLOW_BEHIND + c * FOLLOW_SIDE, player.z - c * FOLLOW_BEHIND - s * FOLLOW_SIDE };
    }

    int m_speciesId = -1;
    bool m_floats = false;
    float m_yaw = 0.0f;
    float m_time = 0.0f;
    float m_workTimer = 0.0f;
    bool m_working = false;
};
