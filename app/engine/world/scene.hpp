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
#include "../../utils/core/randomUtil.hpp"
#include "camera.hpp"
#include "companion.hpp"
#include "chestSpawn.hpp"
#include "captureRules.hpp"
#include "captureTarget.hpp"
#include "chestRules.hpp"
#include "dayCycle.hpp"
#include "inventory.hpp"
#include "player.hpp"
#include "pokeball.hpp"
#include "pokemonStorage.hpp"
#include "resourceSpawn.hpp"
#include "spawnField.hpp"

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
    SpawnField<Chest> chests;
    SpawnField<ResourceNode> nodes;
    PokemonStorage storage;   // equipo (6) y PC
    Companion companion;      // el líder del equipo, que acompaña al jugador y trabaja recursos
    std::string noticeText;   // segunda línea del aviso: recompensa obtenida, pokémon capturado...

    int captures = 0;
    Notice notice = Notice::NONE;
    float noticeTime = 0.0f;
    float throwCooldown = 0.0f;
    bool aimMode = false;     // modo lanzamiento activado con el clic derecho
    int lockedIndex = -1;     // objetivo al que está fijada la cámara (-1 = ninguno)

    // Dos paredes sencillas, más altas que el personaje: bloquean el paso, las pokéballs y la luz.
    explicit Scene(const GameData& gameData = GameData::empty()) : m_data(&gameData) {
        inventory.setData(gameData);
        storage.setData(gameData);
        chests.setup(ChestSpawn::COUNT);
        nodes.setup(ResourceSpawn::COUNT);
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
        for (const Chest& chest : chests.entities) if (chest.closed()) world.props.push_back(chest.solid());
        for (const ResourceNode& node : nodes.entities) if (!node.depleted()) world.props.push_back(node.solid());

        player.aiming = m_aiming;
        player.update(dt, input, camera.yaw(), world);

        throwCooldown = (std::max)(0.0f, throwCooldown - dt);
        noticeTime = (std::max)(0.0f, noticeTime - dt);
        if (m_aiming && input.throwBall && throwCooldown <= 0.0f) throwBall();

        for (CaptureTarget& target : targets) {
            if (target.speciesIndex() < 0) assignSpecies(target);
            handleCaptureEvent(target, target.update(dt, world));
        }
        updateBalls(dt);
        chests.update(dt, ChestSpawn::DELAY,
            [&](Chest& chest) { world.step(chest.body, dt); chest.update(dt); },
            [&](int spot) { return ChestSpawn::make(*m_data, spot); });
        nodes.update(dt, ResourceSpawn::DELAY,
            [&](ResourceNode& node) { world.step(node.body, dt); node.update(dt); },
            [&](int spot) { return ResourceSpawn::make(*m_data, spot); });
        updateCompanion(dt);
        updateInteraction();
        if (input.interact) interact();
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
        s.data = m_data;
        s.aiming = m_aiming;
        s.canLock = anyLockable();
        if (m_missingSkill >= 0) if (const Skill* skill = m_data->skill(m_missingSkill)) {
            s.missingSkill = &skill->name;
            s.missingLevel = m_missingLevel;
        }
        addNameTags(s);
        if (!noticeText.empty()) s.noticeText = &noticeText;
        if (m_interaction.kind == Interaction::CHEST) {
            s.interactVerb = CHEST_VERB;
            s.interactTarget = &m_data->chests[chests.entities[m_interaction.index].typeIndex()].name;
        } else if (m_interaction.kind == Interaction::NODE) {
            const ResourceNodeType& type = m_data->nodes[nodes.entities[m_interaction.index].typeIndex()];
            s.interactVerb = type.action.c_str();
            s.interactTarget = &type.name;
        }
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
    // Nombre sobre cada pokémon visible (los salvajes y el acompañante).
    void addNameTags(GameStatus& s) const {
        constexpr float TAG_RANGE = 30.0f;
        constexpr float TAG_LIFT = 0.35f;
        const auto add = [&](int speciesId, const DirectX::XMFLOAT3& head) {
            const PokemonSpecies* species = m_data->speciesById(speciesId);
            const float dx = head.x - player.body.position.x, dz = head.z - player.body.position.z;
            if (species && dx * dx + dz * dz <= TAG_RANGE * TAG_RANGE) s.nameTags.push_back({ { head.x, head.y + TAG_LIFT, head.z }, &species->name });
        };
        for (const CaptureTarget& target : targets) {
            if (target.hittable()) add(target.speciesId(), { target.body.position.x, target.body.position.y + CaptureTarget::SIZE, target.body.position.z });
        }
        if (companion.active()) add(companion.speciesId(), { companion.body.position.x, companion.body.position.y + Companion::SIZE, companion.body.position.z });
    }

    // --- Porcentaje de captura ---
    bool isBehind(const CaptureTarget& t) const {
        return CaptureRules::isBehind(t.body.position, t.yaw(), player.body.position);
    }

    float chancePercent(const CaptureTarget& t, float ballMultiplier) const {
        return CaptureRules::percent(t.baseChance(), isBehind(t), player.crouched(), ballMultiplier);
    }

    void showNotice(Notice kind, std::string text = {}) {
        notice = kind;
        noticeText = std::move(text);
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
        std::string text;
        if (const PokemonSpecies* species = m_data->speciesById(target.speciesId())) {
            bool sentToPc = false;
            text = species->name + (!storage.add(species->id, sentToPc) ? " (sin espacio)" : sentToPc ? " (enviado al PC)" : "");
        }
        switch (target.throwKind()) {
            case CaptureRules::Throw::LUCKY:       showNotice(Notice::LUCKY, text); break;
            case CaptureRules::Throw::SUPER_LUCKY: showNotice(Notice::SUPER_LUCKY, text); break;
            default:                               showNotice(Notice::CAPTURED, text); break;
        }
    }

    // Especie de un pokémon salvaje que acaba de aparecer, según el peso de cada una.
    void assignSpecies(CaptureTarget& target) const {
        const int index = RandomUtil::weightedIndex(m_data->species, [](const PokemonSpecies& s) { return s.spawnWeight; });
        if (index >= 0) target.setSpecies(index, m_data->species[index].id);
    }

    // El líder del equipo acompaña al jugador y trabaja solo los recursos cercanos cuya habilidad conoce.
    void updateCompanion(float dt) {
        const int lead = storage.leadSpeciesId();
        const PokemonSpecies* species = m_data->speciesById(lead);
        if (lead != companion.speciesId()) companion.set(lead, species && m_data->levitates(*species), player.body.position, camera.yaw());

        ResourceNode* target = nullptr;
        int level = 0;
        if (species) {
            float best = 1.0e9f;
            for (ResourceNode& node : nodes.entities) {
                if (node.depleted()) continue;
                const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
                const int skillLevel = species->skillLevel(type.skillId);
                if (skillLevel < type.level) continue; // no la tiene o no llega al nivel que exige
                const float dx = node.body.position.x - companion.body.position.x;
                const float dz = node.body.position.z - companion.body.position.z;
                const float d2 = dx * dx + dz * dz;
                const float range = Companion::WORK_RANGE + ResourceStyle::look(node.typeId()).half;
                if (d2 <= range * range && d2 < best) {
                    best = d2;
                    target = &node;
                    level = skillLevel;
                }
            }
        }
        if (target) target->companionWorking();
        const float power = target ? Companion::workPower(level, target->teamBonus(false)) : 0.0f;
        if (companion.update(dt, world, player.body.position, camera.yaw(), power) && target) hitNode(*target, false);
    }

    // --- Interacción (cofres y recursos) ---
    // Lo interactuable más cercano de la lista que esté al alcance del jugador: devuelve su índice y deja la distancia² en best.
    template <typename Entity, typename Usable>
    int nearestIn(const std::vector<Entity>& list, Usable usable, float& best) const {
        int index = -1;
        for (size_t i = 0; i < list.size(); ++i) {
            if (!usable(list[i])) continue;
            const float dx = list[i].body.position.x - player.body.position.x;
            const float dz = list[i].body.position.z - player.body.position.z;
            const float d2 = dx * dx + dz * dz;
            if (d2 <= list[i].reach() * list[i].reach() && d2 < best) {
                best = d2;
                index = static_cast<int>(i);
            }
        }
        return index;
    }

    void updateInteraction() {
        float best = 1.0e9f;
        m_interaction = {};
        if (const int i = nearestIn(chests.entities, [](const Chest& c) { return c.closed(); }, best); i >= 0) m_interaction = { Interaction::CHEST, i };
        m_missingSkill = -1;
        if (const int i = nearestIn(nodes.entities, [](const ResourceNode& n) { return !n.depleted(); }, best); i >= 0) {
            const ResourceNodeType& type = m_data->nodes[nodes.entities[i].typeIndex()];
            if (inventory.skillLevel(type.skillId) >= type.level) m_interaction = { Interaction::NODE, i };
            else {
                m_missingSkill = type.skillId; // sin herramienta de ese nivel: solo un pokémon que llegue al nivel puede trabajarlo
                m_missingLevel = type.level;
            }
        }
    }

    void interact() {
        if (m_interaction.kind == Interaction::CHEST) openChest(chests.entities[m_interaction.index]);
        else if (m_interaction.kind == Interaction::NODE) {
            ResourceNode& node = nodes.entities[m_interaction.index];
            hitNode(node, true, inventory.skillSpeed(m_data->nodes[node.typeIndex()].skillId));
        }
    }

    // Suma 'quantity' unidades de un objeto al inventario y lo anuncia.
    void giveReward(int itemId, int quantity) {
        if (!inventory.add(itemId, quantity)) return;
        const Item* item = m_data->item(itemId);
        std::string text = std::to_string(quantity) + "x " + (item ? m_data->itemName(*item) : std::string("?"));
        showNotice(Notice::REWARD, std::move(text));
    }

    // Abre el cofre: una recompensa al azar de las de su tipo, directa al inventario.
    void openChest(Chest& chest) {
        const ChestReward* reward = ChestRules::pickReward(m_data->chests[chest.typeIndex()]);
        chest.open();
        if (reward) giveReward(reward->itemId, reward->quantity);
    }

    // Un golpe al recurso; al agotarlo da su material.
    void hitNode(ResourceNode& node, bool byPlayer, float speed = 1.0f) {
        if (!node.hit(byPlayer, speed) || !node.depleted()) return;
        const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
        const Item* item = m_data->materialItem(type.materialId);
        if (item) giveReward(item->id, RandomUtil::integer(type.minYield, type.maxYield));
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

    struct Interaction {
        enum Kind { NONE, CHEST, NODE } kind = NONE;
        int index = -1;
    };

    // Hay algún pokémon al alcance al que fijar la cámara (o ya hay uno fijado).
    bool anyLockable() const {
        if (lockedIndex >= 0) return true;
        for (int i = 0; i < TARGET_COUNT; ++i) if (targets[i].hittable() && distanceXZ(targets[i]) <= RANGE) return true;
        return false;
    }

    static constexpr const char* CHEST_VERB = "Abrir";

    const GameData* m_data;
    int m_missingSkill = -1;   // habilidad que falta para el recurso cercano (-1 = ninguna)
    int m_missingLevel = 0;    // nivel que exige ese recurso
    Interaction m_interaction; // lo que el jugador puede usar ahora mismo (cofre o recurso cercano)
    bool m_aiming = false; // modo lanzamiento (clic derecho) o L2 mantenido
    int m_aimTarget = -1;  // pokémon al que apunta la cruceta dentro del alcance
};
