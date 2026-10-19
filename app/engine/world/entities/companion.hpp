#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"
#include "ballTransfer.hpp"

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
    static constexpr float SEARCH_RANGE = 12.0f;  // radio alrededor del jugador donde busca cosas que recoger o trabajar
    static constexpr float AVOID_PROBE = 1.6f;    // a qué distancia por delante mira si hay un obstáculo
    static constexpr float AVOID_MIN_GOAL = 5.0f; // no esquiva cuando ya está junto a su objetivo (el obstáculo puede ser él mismo)
    static constexpr float PROBE_HEIGHT = 0.35f;  // altura sobre el suelo a la que mira
    static constexpr float APPROACH = 1.5f;       // se acerca hasta esta distancia de su objetivo
    static constexpr float WORK_INTERVAL = 1.4f;  // segundos por golpe con power 1
    static constexpr float LEVEL_SPEEDUP = 0.25f; // cada nivel de recolección por encima del 1 trabaja un 25 % más rápido
    static constexpr float HOVER_HEIGHT = 1.3f;   // altura sobre el jugador al levitar

    // Cambio de pokémon (BallTransfer): el que está fuera vuelve a su pokéball y después sale el nuevo de la suya.
    static constexpr float HAND_HEIGHT = 1.0f;    // altura de la mano del jugador
    static constexpr float HOVER_OVER = 1.0f;     // la bola queda suspendida a esta altura sobre el pokémon

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

    // Escala del pokémon dibujado (0..1): sale de su bola o vuelve a ella por el rayo.
    float scale() const { return m_phase == Phase::NONE ? 1.0f : m_transfer.emerge(); }

    // El cambio en curso (nullptr si no hay) y dónde se dibuja el pokémon mientras sale o entra.
    const BallTransfer* transfer() const { return m_phase == Phase::NONE ? nullptr : &m_transfer; }
    DirectX::XMFLOAT3 center() const { return { body.position.x, body.position.y + SIZE * 0.5f, body.position.z }; }
    DirectX::XMFLOAT3 drawCenter() const { return m_phase == Phase::NONE ? center() : m_transfer.creatureCenter(center()); }
    bool changing() const { return m_phase != Phase::NONE; }

    // Cambia de pokémon con su animación: si hay uno fuera vuelve a su bola; después sale el nuevo de la suya.
    void swap(int speciesId, bool floats, int ballType, const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        m_next = { speciesId, floats, ballType };
        m_cameraYaw = cameraYaw;
        if (m_phase == Phase::RECALL) return; // ya se está guardando: saldrá el último elegido
        if (active()) {
            m_phase = Phase::RECALL;
            m_transfer.start(BallTransfer::Mode::ABSORB, cameraYaw, hand(playerPosition), m_ballType);
        } else beginSend(playerPosition, cameraYaw);
    }

    // Cambia de pokémon (-1 = ninguno): aparece junto al jugador.
    void set(int speciesId, bool floats, const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        m_phase = Phase::NONE;
        m_transfer.stop();
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
        m_cameraYaw = cameraYaw;
        if (m_phase != Phase::NONE) {
            animate(dt, world, playerPosition, cameraYaw);
            return false;
        }

        const DirectX::XMFLOAT2 spot = goal ? DirectX::XMFLOAT2{ goal->x, goal->z } : followSpot(playerPosition, cameraYaw);
        const float dx = spot.x - body.position.x, dz = spot.y - body.position.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        const float stop = goal ? APPROACH : 0.3f;
        float heading = std::atan2(dx, dz);
        if (goal && distance > AVOID_MIN_GOAL) heading = avoid(world, heading);
        if (!goal && distance > TELEPORT_DISTANCE) {
            body.position = { spot.x, playerPosition.y, spot.y };
            body.velocity = { 0.0f, 0.0f, 0.0f };
            body.onGround = false;
        } else if (distance > stop) {
            const float speed = (std::min)(SPEED, (distance - stop + 0.3f) * 3.0f);
            body.velocity.x = std::sin(heading) * speed;
            body.velocity.z = std::cos(heading) * speed;
            m_yaw = heading;
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
    struct Next { int speciesId; bool floats; int ballType; };

    static DirectX::XMFLOAT3 hand(const DirectX::XMFLOAT3& player) { return { player.x, player.y + HAND_HEIGHT, player.z }; }

    void beginSend(const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        set(m_next.speciesId, m_next.floats, playerPosition, cameraYaw);
        m_ballType = m_next.ballType;
        m_phase = Phase::SEND;
        m_transfer.start(BallTransfer::Mode::RELEASE, cameraYaw, hand(playerPosition), m_ballType);
    }

    // Avanza el cambio: la bola queda suspendida sobre el pokémon (que no se mueve mientras tanto).
    void animate(float dt, const Physics::World& world, const DirectX::XMFLOAT3& playerPosition, float cameraYaw) {
        const DirectX::XMFLOAT3 over = { body.position.x, body.position.y + SIZE * 0.5f + HOVER_OVER, body.position.z };
        m_transfer.update(dt, over, hand(playerPosition));
        body.velocity = { 0.0f, 0.0f, 0.0f };
        world.step(body, dt);
        if (m_transfer.active()) return;
        if (m_phase == Phase::RECALL) beginSend(playerPosition, cameraYaw);
        else m_phase = Phase::NONE;
    }

    // Rumbo libre de obstáculos más cercano al deseado: si hay algo delante, prueba cada vez más a los lados (primero hacia el
    // lado que ya usaba, para no vacilar) y sigue por el primero que está despejado.
    float avoid(const Physics::World& world, float heading) {
        const DirectX::XMFLOAT3 from = { body.position.x, body.position.y + PROBE_HEIGHT, body.position.z };
        const auto clear = [&](float angle) {
            const DirectX::XMFLOAT3 to = { from.x + std::sin(angle) * AVOID_PROBE, from.y, from.z + std::cos(angle) * AVOID_PROBE };
            return !world.blocked(from, to);
        };
        if (clear(heading)) {
            m_avoidSide = 0;
            return heading;
        }
        const int first = m_avoidSide != 0 ? m_avoidSide : 1;
        for (const float offset : { 0.5f, 1.0f, 1.5f, 2.0f }) {
            for (const int side : { first, -first }) {
                if (!clear(heading + side * offset)) continue;
                m_avoidSide = side;
                return heading + side * offset;
            }
        }
        return heading;
    }

    // Punto donde se coloca: detrás y a la derecha del jugador respecto a la cámara.
    static DirectX::XMFLOAT2 followSpot(const DirectX::XMFLOAT3& player, float cameraYaw) {
        const float s = std::sin(cameraYaw), c = std::cos(cameraYaw);
        return { player.x - s * FOLLOW_BEHIND + c * FOLLOW_SIDE, player.z - c * FOLLOW_BEHIND - s * FOLLOW_SIDE };
    }

    int m_speciesId = -1;
    bool m_floats = false;
    float m_yaw = 0.0f;
    int m_avoidSide = 0;      // lado (±1) por el que rodea un obstáculo; 0 = camino libre
    float m_time = 0.0f;
    float m_workTimer = 0.0f;
    bool m_working = false;
    Phase m_phase = Phase::NONE;
    float m_cameraYaw = 0.0f;
    BallTransfer m_transfer;
    int m_ballType = 1;                       // tipo de la pokéball del pokémon que está fuera
    Next m_next = { -1, false, 1 };
};
