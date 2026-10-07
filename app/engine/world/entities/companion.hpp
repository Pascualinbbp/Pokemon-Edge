#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"
#include "pokeball.hpp"

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
    static constexpr float SEARCH_RANGE = 16.0f;  // radio alrededor del jugador donde busca cosas que recoger o trabajar
    static constexpr float APPROACH = 1.5f;       // se acerca hasta esta distancia de su objetivo
    static constexpr float WORK_INTERVAL = 1.4f;  // segundos por golpe con power 1
    static constexpr float LEVEL_SPEEDUP = 0.25f; // cada nivel de recolección por encima del 1 trabaja un 25 % más rápido
    static constexpr float HOVER_HEIGHT = 1.3f;   // altura sobre el jugador al levitar

    // Cambio de pokémon: el que está fuera vuelve a su pokéball y el nuevo sale de la suya.
    static constexpr float RECALL_TIME = 0.45f;   // el pokémon se encoge hacia su bola y esta vuela a la mano del jugador
    static constexpr float THROW_TIME = 0.45f;    // la bola del nuevo vuela hasta su sitio
    static constexpr float APPEAR_TIME = 0.3f;    // la bola se abre y el pokémon crece
    static constexpr float HAND_HEIGHT = 1.0f;    // altura de la mano del jugador
    static constexpr float THROW_ARC = 1.2f;      // altura máxima del lanzamiento

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

    // Escala del pokémon dibujado (0..1): se encoge al volver a la bola y crece al salir de ella.
    float scale() const {
        switch (m_phase) {
            case Phase::RECALL: return 1.0f - (std::min)(m_phaseTime / RECALL_TIME, 1.0f);
            case Phase::SEND:   return (std::clamp)((m_phaseTime - THROW_TIME) / APPEAR_TIME, 0.0f, 1.0f);
            default:            return 1.0f;
        }
    }

    // La pokéball que se ve durante el cambio (nullptr si no hay cambio en curso) y su escala.
    const Pokeball* ball() const { return m_phase == Phase::NONE ? nullptr : &m_ball; }
    float ballScale() const { return m_ballScale; }
    bool changing() const { return m_phase != Phase::NONE; }

    // Cambia de pokémon con su animación: si hay uno fuera vuelve a su bola; después sale el nuevo de la suya.
    void swap(int speciesId, bool floats, const DirectX::XMFLOAT3& ballColor, const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        m_next = { speciesId, floats, ballColor };
        if (active()) {
            m_phase = Phase::RECALL;
            m_phaseTime = 0.0f;
            m_ball.color = m_color;
        } else beginSend(playerPosition, cameraYaw);
    }

    // Cambia de pokémon (-1 = ninguno): aparece junto al jugador.
    void set(int speciesId, bool floats, const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        m_phase = Phase::NONE;
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
    // completa un golpe. Con 'goal' va hacia ese punto (aunque se separe del jugador) en vez de seguirlo.
    bool update(float dt, const Physics::World& world, const DirectX::XMFLOAT3& playerPosition, float cameraYaw, float power,
                const DirectX::XMFLOAT3* goal = nullptr) {
        if (!active()) return false;
        m_time += dt;
        if (m_phase != Phase::NONE) {
            animate(dt, world, playerPosition, cameraYaw);
            return false;
        }

        const DirectX::XMFLOAT2 spot = goal ? DirectX::XMFLOAT2{ goal->x, goal->z } : followSpot(playerPosition, cameraYaw);
        const float dx = spot.x - body.position.x, dz = spot.y - body.position.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        const float stop = goal ? APPROACH : 0.3f;
        if (!goal && distance > TELEPORT_DISTANCE) {
            body.position = { spot.x, playerPosition.y, spot.y };
            body.velocity = { 0.0f, 0.0f, 0.0f };
            body.onGround = false;
        } else if (distance > stop) {
            const float speed = (std::min)(SPEED, (distance - stop + 0.3f) * 3.0f);
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
    enum class Phase { NONE, RECALL, SEND };
    struct Next { int speciesId; bool floats; DirectX::XMFLOAT3 color; };

    void beginSend(const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        set(m_next.speciesId, m_next.floats, playerPosition, cameraYaw);
        m_color = m_next.color;
        m_ball.color = m_color;
        m_phase = Phase::SEND;
        m_phaseTime = 0.0f;
        m_throwFrom = { playerPosition.x, playerPosition.y + HAND_HEIGHT, playerPosition.z };
        m_ballScale = 1.0f;
        m_ball.body.position = m_throwFrom;
    }

    // Avanza la animación del cambio y coloca la pokéball visible.
    void animate(float dt, const Physics::World& world, const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        m_phaseTime += dt;
        const DirectX::XMFLOAT3 hand = { playerPosition.x, playerPosition.y + HAND_HEIGHT, playerPosition.z };
        if (m_phase == Phase::RECALL) {
            const float u = (std::min)(m_phaseTime / RECALL_TIME, 1.0f);
            const DirectX::XMFLOAT3 from = { body.position.x, body.position.y + SIZE * 0.5f, body.position.z };
            m_ball.body.position = { from.x + (hand.x - from.x) * u, from.y + (hand.y - from.y) * u, from.z + (hand.z - from.z) * u };
            m_ballScale = (std::min)(1.0f, u * 4.0f);
            if (m_phaseTime >= RECALL_TIME) beginSend(playerPosition, cameraYaw);
            return;
        }

        // SEND: la bola vuela en arco hasta el sitio del pokémon, se abre y este crece.
        const DirectX::XMFLOAT3 target = { body.position.x, body.position.y + SIZE * 0.5f, body.position.z };
        const float u = (std::min)(m_phaseTime / THROW_TIME, 1.0f);
        m_ball.body.position = { m_throwFrom.x + (target.x - m_throwFrom.x) * u,
                                 m_throwFrom.y + (target.y - m_throwFrom.y) * u + std::sin(u * 3.14159265f) * THROW_ARC,
                                 m_throwFrom.z + (target.z - m_throwFrom.z) * u };
        m_ballScale = m_phaseTime <= THROW_TIME ? 1.0f : (std::max)(0.0f, 1.0f - (m_phaseTime - THROW_TIME) / APPEAR_TIME);
        body.velocity = { 0.0f, 0.0f, 0.0f };
        world.step(body, dt);
        if (m_phaseTime >= THROW_TIME + APPEAR_TIME) m_phase = Phase::NONE;
    }

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
    Phase m_phase = Phase::NONE;
    float m_phaseTime = 0.0f;
    float m_ballScale = 0.0f;
    Pokeball m_ball;                          // solo para dibujar el cambio
    DirectX::XMFLOAT3 m_color = { 0.86f, 0.16f, 0.16f }; // color de la pokéball del pokémon que está fuera
    DirectX::XMFLOAT3 m_throwFrom = {};
    Next m_next = { -1, false, { 1.0f, 1.0f, 1.0f } };
};
