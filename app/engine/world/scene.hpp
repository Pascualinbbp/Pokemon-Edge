#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../core/gameStatus.hpp"
#include "../core/input.hpp"
#include "../physics/physicsWorld.hpp"
#include "../../models/gameData.hpp"
#include "camera.hpp"
#include "chestField.hpp"
#include "captureRules.hpp"
#include "captureTarget.hpp"
#include "chestRules.hpp"
#include "dayCycle.hpp"
#include "inventory.hpp"
#include "player.hpp"
#include "pokeball.hpp"

struct Scene {
    static constexpr float HALF_SIZE = Physics::World::HALF_SIZE;
    static constexpr int TARGET_COUNT = 4;

    // Lanzamiento
    static constexpr float THROW_SPEED = 40.0f;       // m/s: trayectoria tensa (la pokéball cae con la mitad de gravedad)
    static constexpr float THROW_COOLDOWN = 0.45f;    // segundos entre lanzamientos
    static constexpr float MAX_AIM_DISTANCE = 25.0f;  // si el rayo de apuntado no choca con nada, se apunta a esta distancia
    static constexpr float RANGE = 24.0f;             // alcance del jugador: porcentaje visible y fijado de cámara
    static constexpr float LOCK_RELEASE_MARGIN = 3.0f; // el fijado se mantiene un poco más allá del alcance
    static constexpr float SPAWN_SIDE = 0.35f;        // la pokéball sale por el hombro derecho
    static constexpr float SPAWN_FORWARD = 0.5f;
    static constexpr float SPAWN_HEIGHT = 1.1f;
    static constexpr size_t MAX_BALLS = 12;
    static constexpr float MAX_SUBSTEP = 1.0f / 120.0f; // paso máximo de la física de los proyectiles
    static constexpr float NOTICE_TIME = 2.0f;

    Physics::World world;
    Player player;
    Camera camera;
    std::array<CaptureTarget, TARGET_COUNT> targets = { CaptureTarget(0), CaptureTarget(1), CaptureTarget(2), CaptureTarget(3) };
    std::vector<Pokeball> balls;
    DayCycle dayCycle;
    Inventory inventory;
    ChestField chests;
    std::string rewardText; // recompensa del último cofre abierto

    int captures = 0;
    Notice notice = Notice::NONE;
    float noticeTime = 0.0f;
    float throwCooldown = 0.0f;
    bool aimMode = false;     // modo lanzamiento activado con el clic derecho
    int lockedIndex = -1;     // objetivo al que está fijada la cámara (-1 = ninguno)

    // Dos paredes sencillas, más altas que el personaje: bloquean el paso, las pokéballs y la luz.
    explicit Scene(const GameData& gameData = GameData::empty()) : m_data(&gameData) {
        inventory.setData(gameData);
        chests.setup(gameData);
        world.obstacles = {
            { {  6.0f, 1.75f,  5.0f }, { 4.0f, 1.75f, 0.5f } },
            { { -7.0f, 1.75f, -4.0f }, { 0.5f, 1.75f, 4.0f } },
        };
    }

    // Devuelve true si hay que pausar el juego (ESC fuera del modo lanzamiento).
    bool update(float dt, const InputState& input) {
        bool pause = false;
        if (input.escape) {
            if (aimMode) aimMode = false;
            else pause = true;
        }
        if (input.aimToggle) aimMode = !aimMode;
        m_aiming = aimMode || input.aimHold;
        if (m_aiming && input.ballSwitch != 0) inventory.cycle(input.ballSwitch);

        if (input.lockCancel) lockedIndex = -1;
        else if (input.lockTap) cycleLock();
        validateLock();

        camera.update(dt, m_aiming);
        if (lockedIndex >= 0) camera.trackToward(player.body.position, targets[lockedIndex].center(), dt);
        else camera.rotate(input.lookX, input.lookY);

        // Los pokémon son sólidos para quien los pisa: el jugador los rodea igual que a las paredes.
        world.creatures.clear();
        for (const CaptureTarget& target : targets) if (target.hittable()) world.creatures.push_back(target.solid());
        world.props.clear();
        for (const Chest& chest : chests.chests) if (chest.closed()) world.props.push_back(chest.solid());

        player.aiming = m_aiming;
        player.update(dt, input, camera.yaw(), world);

        throwCooldown = (std::max)(0.0f, throwCooldown - dt);
        noticeTime = (std::max)(0.0f, noticeTime - dt);
        if (m_aiming && input.throwBall && throwCooldown <= 0.0f) throwBall();

        for (CaptureTarget& target : targets) handleCaptureEvent(target, target.update(dt, world));
        updateBalls(dt);
        chests.update(dt, dayCycle.day(), world);
        updateNearbyChest();
        if (input.interact && m_nearChest >= 0) openChest(m_nearChest);
        updateAimInfo();
        return pause;
    }

    DirectX::XMMATRIX getViewMatrix() const {
        return camera.viewMatrix(player.body.position);
    }

    GameStatus status() const {
        GameStatus s;
        s.aimBlend = camera.aimBlend();
        s.captures = captures;
        s.notice = noticeTime > 0.0f ? notice : Notice::NONE;
        s.locked = lockedIndex >= 0;
        s.inventory = &inventory;
        if (m_nearChest >= 0) s.nearbyChest = &m_data->chests[chests.chests[m_nearChest].typeIndex()];
        if (notice == Notice::REWARD) s.rewardText = &rewardText;
        if (m_aimTarget >= 0) {
            const CaptureTarget& t = targets[m_aimTarget];
            s.hasAimTarget = true;
            s.behind = isBehind(t);
            s.hidden = player.crouched();
            s.chancePercent = static_cast<int>(std::lround(chancePercent(t, inventory.captureMultiplier())));
        }
        return s;
    }

    private:
    // --- Porcentaje de captura ---
    bool isBehind(const CaptureTarget& t) const {
        return CaptureRules::isBehind(t.body.position, t.yaw(), player.body.position);
    }

    float chancePercent(const CaptureTarget& t, float ballMultiplier) const {
        return CaptureRules::percent(t.baseChance(), isBehind(t), player.crouched(), ballMultiplier);
    }

    void showNotice(Notice kind) {
        notice = kind;
        noticeTime = NOTICE_TIME;
    }

    // El resultado se decide al tocar la bola al pokémon, pero se anuncia cuando la animación lo revela.
    void handleCaptureEvent(const CaptureTarget& target, CaptureTarget::Event event) {
        if (event == CaptureTarget::Event::NONE) return;
        if (event == CaptureTarget::Event::ESCAPED) {
            showNotice(Notice::ESCAPED);
            return;
        }

        ++captures;
        switch (target.throwKind()) {
            case CaptureRules::Throw::LUCKY:       showNotice(Notice::LUCKY); break;
            case CaptureRules::Throw::SUPER_LUCKY: showNotice(Notice::SUPER_LUCKY); break;
            default:                               showNotice(Notice::CAPTURED); break;
        }
    }

    // --- Cofres ---
    // Cofre cerrado más cercano dentro del alcance del jugador (-1 si ninguno).
    void updateNearbyChest() {
        m_nearChest = -1;
        float best = Chest::INTERACT_RANGE * Chest::INTERACT_RANGE;
        for (size_t i = 0; i < chests.chests.size(); ++i) {
            const Chest& chest = chests.chests[i];
            if (!chest.closed()) continue;
            const float dx = chest.body.position.x - player.body.position.x;
            const float dz = chest.body.position.z - player.body.position.z;
            const float d2 = dx * dx + dz * dz;
            if (d2 <= best) {
                best = d2;
                m_nearChest = static_cast<int>(i);
            }
        }
    }

    // Abre el cofre: una recompensa al azar de las de su tipo, directa al inventario.
    void openChest(int index) {
        Chest& chest = chests.chests[index];
        const ChestReward* reward = ChestRules::pickReward(m_data->chests[chest.typeIndex()]);
        chest.open();
        if (!reward || !inventory.add(reward->itemId, reward->quantity)) return;

        const Item* item = m_data->item(reward->itemId);
        rewardText = std::to_string(reward->quantity) + "x " + (item ? item->name : std::string("?"));
        showNotice(Notice::REWARD);
    }

    // --- Fijado de cámara ---
    float distanceXZ(const CaptureTarget& t) const {
        return std::hypot(t.body.position.x - player.body.position.x, t.body.position.z - player.body.position.z);
    }

    // Ángulo horizontal del objetivo respecto a hacia donde mira la cámara (positivo = a la derecha).
    float relativeAngle(const CaptureTarget& t) const {
        const float dx = t.body.position.x - player.body.position.x;
        const float dz = t.body.position.z - player.body.position.z;
        return DirectX::XMScalarModAngle(std::atan2(dx, dz) - camera.yaw());
    }

    // Objetivos disponibles para fijar (vivos y dentro del alcance).
    std::vector<int> lockCandidates(int exclude) const {
        std::vector<int> result;
        for (int i = 0; i < TARGET_COUNT; ++i) {
            if (i != exclude && targets[i].hittable() && distanceXZ(targets[i]) <= RANGE) result.push_back(i);
        }
        return result;
    }

    int closestToCenter(const std::vector<int>& candidates) const {
        int best = candidates.front();
        for (const int i : candidates) {
            if (std::fabs(relativeAngle(targets[i])) < std::fabs(relativeAngle(targets[best]))) best = i;
        }
        return best;
    }

    // Una pulsación: fija el objetivo más cercano al centro; si ya hay uno, pasa al siguiente (hacia la derecha).
    void cycleLock() {
        std::vector<int> candidates = lockCandidates(-1);
        if (candidates.empty()) return;
        if (lockedIndex < 0) {
            lockedIndex = closestToCenter(candidates);
            return;
        }

        std::sort(candidates.begin(), candidates.end(),
            [this](int a, int b) { return relativeAngle(targets[a]) < relativeAngle(targets[b]); });
        const auto it = std::find(candidates.begin(), candidates.end(), lockedIndex);
        if (it == candidates.end()) lockedIndex = closestToCenter(candidates);
        else lockedIndex = (it + 1 == candidates.end()) ? candidates.front() : *(it + 1);
    }

    // Si el objetivo fijado se captura o queda fuera de alcance, salta a otro cercano; si no hay, se suelta.
    void validateLock() {
        if (lockedIndex < 0) return;
        const CaptureTarget& t = targets[lockedIndex];
        if (t.hittable() && distanceXZ(t) <= RANGE + LOCK_RELEASE_MARGIN) return;

        const std::vector<int> candidates = lockCandidates(lockedIndex);
        lockedIndex = candidates.empty() ? -1 : closestToCenter(candidates);
    }

    // --- Apuntado ---
    // Pokémon más cercano que atraviesa el rayo. Devuelve su índice (-1 si ninguno) y la distancia al impacto.
    int nearestHit(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& dir, float& distance, bool inRangeOnly) const {
        int best = -1;
        distance = 1.0e9f;
        for (int i = 0; i < TARGET_COUNT; ++i) {
            float t = 0.0f;
            if (inRangeOnly && distanceXZ(targets[i]) > RANGE) continue;
            if (targets[i].raycast(eye, dir, t) && t < distance) {
                distance = t;
                best = i;
            }
        }
        return best;
    }

    // Pokémon al que apunta ahora la cruceta (para mostrar su porcentaje).
    void updateAimInfo() {
        m_aimTarget = -1;
        if (!m_aiming) return;
        float distance;
        m_aimTarget = nearestHit(camera.eye(player.body.position), camera.forward(), distance, true);
    }

    // Lanza la pokéball equipada hacia el punto que señala la cruceta (centro de la pantalla).
    void throwBall() {
        using namespace DirectX;
        throwCooldown = THROW_COOLDOWN;
        if (!inventory.canThrow()) {
            showNotice(Notice::OUT_OF_STOCK);
            return;
        }

        const XMFLOAT3 eye = camera.eye(player.body.position);
        const XMFLOAT3 dir = camera.forward();

        // Punto apuntado: primer impacto del rayo de la cámara con el suelo o un pokémon.
        float distance = MAX_AIM_DISTANCE;
        const float ground = world.groundHeight(eye.x, eye.z);
        if (dir.y < -1.0e-4f) distance = (std::min)(distance, (std::max)((ground - eye.y) / dir.y, 0.0f));
        float hit = 0.0f;
        if (nearestHit(eye, dir, hit, false) >= 0) distance = (std::min)(distance, hit);
        const XMFLOAT3 aim = { eye.x + dir.x * distance, eye.y + dir.y * distance, eye.z + dir.z * distance };

        float sinYaw, cosYaw;
        XMScalarSinCos(&sinYaw, &cosYaw, camera.yaw());
        const XMFLOAT3& p = player.body.position;

        const PokeballType& type = inventory.selectedBall().type;
        Pokeball ball;
        ball.captureMultiplier = type.captureMultiplier;
        ball.color = PokeballStyle::color(type.id);
        ball.body.position = { p.x + cosYaw * SPAWN_SIDE + sinYaw * SPAWN_FORWARD,
                               p.y + SPAWN_HEIGHT * player.heightScale(),
                               p.z - sinYaw * SPAWN_SIDE + cosYaw * SPAWN_FORWARD };
        ball.body.velocity = launchVelocity(ball.body.position, aim, dir);

        if (balls.size() >= MAX_BALLS) balls.erase(balls.begin());
        balls.push_back(ball);
        inventory.consume();
    }

    // Velocidad inicial (módulo THROW_SPEED) para que la parábola pase por 'aim' (trayectoria baja).
    // Si el punto está fuera de alcance, se lanza a 45° (máximo alcance). Si está pegado al jugador,
    // se lanza en línea recta según la cámara.
    static DirectX::XMFLOAT3 launchVelocity(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& aim,
                                            const DirectX::XMFLOAT3& cameraDir) {
        const float dx = aim.x - from.x;
        const float dz = aim.z - from.z;
        const float dy = aim.y - from.y;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (d < 0.5f) return { cameraDir.x * THROW_SPEED, cameraDir.y * THROW_SPEED, cameraDir.z * THROW_SPEED };

        const float v2 = THROW_SPEED * THROW_SPEED;
        const float g = Physics::World::GRAVITY * Pokeball::GRAVITY_SCALE;
        const float discriminant = v2 * v2 - g * (g * d * d + 2.0f * dy * v2);
        const float tanA = discriminant >= 0.0f ? (v2 - std::sqrt(discriminant)) / (g * d) : 1.0f;

        const float cosA = 1.0f / std::sqrt(1.0f + tanA * tanA);
        const float sinA = tanA * cosA;
        return { dx / d * THROW_SPEED * cosA, THROW_SPEED * sinA, dz / d * THROW_SPEED * cosA };
    }

    // Física de las pokéballs en pasos pequeños (así una bola rápida no atraviesa un objetivo).
    void updateBalls(float dt) {
        if (balls.empty()) return;

        const int steps = (std::max)(1, static_cast<int>(std::ceil(dt / MAX_SUBSTEP)));
        const float h = dt / static_cast<float>(steps);
        for (int i = 0; i < steps; ++i) {
            for (Pokeball& ball : balls) {
                if (ball.expired()) continue;

                world.step(ball.body, h);
                ball.age += h;
                for (CaptureTarget& target : targets) {
                    if (!target.hitBy(ball.body.position, Pokeball::RADIUS + Pokeball::CONTACT_MARGIN)) continue;
                    target.beginCapture(ball, CaptureRules::roll(chancePercent(target, ball.captureMultiplier)));
                    ball.age = Pokeball::LIFETIME; // la bola pasa a ser la de la animación de captura
                    break;
                }
            }
        }
        balls.erase(std::remove_if(balls.begin(), balls.end(), [](const Pokeball& b) { return b.expired(); }), balls.end());
    }

    const GameData* m_data;
    int m_nearChest = -1;  // cofre al alcance del jugador (índice en chests.chests)
    bool m_aiming = false; // modo lanzamiento (clic derecho) o L2 mantenido
    int m_aimTarget = -1;  // pokémon al que apunta la cruceta dentro del alcance
};
