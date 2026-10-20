#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../core/gameStatus.hpp"
#include "../core/input.hpp"
#include "../physics/physicsWorld.hpp"
#include "../../models/gameData.hpp"
#include "../../utils/core/randomUtil.hpp"
#include "camera.hpp"
#include "entities/companion.hpp"
#include "spawn/chestField.hpp"
#include "rules/captureRules.hpp"
#include "rules/companionRules.hpp"
#include "rules/damageRules.hpp"
#include "entities/captureTarget.hpp"
#include "rules/chestRules.hpp"
#include "rules/evRules.hpp"
#include "state/playerProgress.hpp"
#include "rules/pokemonRules.hpp"
#include "state/researchMachine.hpp"
#include "spawn/groundSpawn.hpp"
#include "dayCycle.hpp"
#include "habitat/habitatMap.hpp"
#include "habitat/weatherSystem.hpp"
#include "spawn/wildSpawn.hpp"
#include "spawn/wildField.hpp"
#include "state/exploration.hpp"
#include "state/fishing.hpp"
#include "state/inventory.hpp"
#include "entities/player.hpp"
#include "entities/playerAction.hpp"
#include "entities/pokeball.hpp"
#include "state/pokemonStorage.hpp"
#include "spawn/resourceSpawn.hpp"
#include "state/playerHealth.hpp"
#include "spawn/spawnField.hpp"

struct Scene {
    static constexpr float HALF_SIZE = Physics::World::HALF_SIZE;

    // Lanzamiento
    static constexpr float THROW_SPEED = 40.0f;       // m/s: trayectoria tensa (la pokéball cae con la mitad de gravedad)
    static constexpr float THROW_COOLDOWN = 0.45f;    // segundos entre lanzamientos
    static constexpr float MAX_AIM_DISTANCE = 25.0f;  // si el rayo de apuntado no choca con nada, se apunta a esta distancia
    static constexpr float RANGE = 24.0f;             // alcance del jugador: porcentaje visible y fijado de cámara
    static constexpr float LOCK_RELEASE_MARGIN = 3.0f; // el fijado se mantiene un poco más allá del alcance
    static constexpr float LOCK_HIDDEN_GRACE = 0.8f;   // segundos que el fijado aguanta con el pokémon tapado
    static constexpr float CHEST_HEIGHT = 1.2f;        // altura desde la que el jugador ve a los pokémon
    static constexpr float SPAWN_SIDE = 0.35f;        // la pokéball sale por el hombro derecho
    static constexpr float SPAWN_FORWARD = 0.5f;
    static constexpr float SPAWN_HEIGHT = 1.1f;
    static constexpr float THROW_DELAY = 0.18f;       // la bola sale de la mano a mitad de la animación de lanzar
    static constexpr float AIM_STEP = 0.5f;           // paso de la marcha del rayo de apuntado contra el relieve
    static constexpr float TIDE_AMPLITUDE = 0.9f;     // la marea sube y baja esta altura (m) en el mar
    static constexpr float TIDE_PERIOD = 300.0f;      // segundos que dura un ciclo de marea
    static constexpr size_t MAX_BALLS = 12;
    static constexpr float MAX_SUBSTEP = 1.0f / 120.0f; // paso máximo de la física de los proyectiles
    static constexpr float NOTICE_TIME = 2.0f;
    static constexpr float CONCEAL_RADIUS = 1.6f;      // a esta distancia de un matorral el jugador queda oculto
    static constexpr float PROP_RANGE = 40.0f;         // solo los nodos tan cerca del jugador cuentan como sólidos

    Physics::World world;
    Player player;
    Camera camera;
    WildField wild;           // pokémon salvajes por chunks: solo los cercanos están cargados
    std::vector<Pokeball> balls;
    DayCycle dayCycle;
    HabitatMap habitats;      // zonas de cada hábitat, generadas al empezar la partida
    WeatherSystem weather;    // clima de cada hábitat
    Inventory inventory;
    ChestField chests;        // cofres: aparecen dinámicamente según el hábitat
    std::vector<ResourceNode> nodes; // materiales de recolección, colocados al generar el mundo
    Exploration exploration;  // lo que el jugador ha explorado (el mapa grande lo muestra)
    Fishing fishing;          // pesca: corcho, picada y pokémon enganchado
    SpawnField<GroundItem> groundItems;
    PokemonStorage storage;   // equipo (6) y PC
    PlayerProgress progress;  // nivel, experiencia y medallas del jugador
    PlayerHealth health;      // vida del jugador (caídas, ahogamiento)
    ResearchMachine machine;  // máquina de investigación (fija)
    bool researchRequested = false; // el jugador ha usado la máquina: la interfaz abre su pantalla
    Companion companion;      // el líder del equipo, que acompaña al jugador y trabaja recursos
    std::string noticeText;   // segunda línea del aviso: recompensa obtenida, pokémon capturado...

    Notice notice = Notice::NONE;
    float noticeTime = 0.0f;
    float throwCooldown = 0.0f;
    bool aimMode = false;     // modo lanzamiento activado con el clic derecho
    CaptureTarget* lockedTarget = nullptr; // objetivo al que está fijada la cámara (nullptr = ninguno)

    // Dos paredes sencillas, más altas que el personaje: bloquean el paso, las pokéballs y la luz.
    explicit Scene(const GameData& gameData = GameData::empty(), unsigned worldSeed = RandomUtil::freshSeed()) : m_data(&gameData) {
        habitats.generate(gameData, worldSeed);
        habitats.applyTo(world);
        weather.setup(gameData);
        inventory.setData(gameData);
        storage.setData(gameData);
        groundItems.setup(GroundSpawn::COUNT);
        constexpr float BASE = Physics::World::START_HEIGHT; // el inicio es llano: las paredes se apoyan en él
        world.obstacles = {
            { {  6.0f, BASE + 1.75f,  5.0f }, { 4.0f, 1.75f, 0.5f } },
            { { -7.0f, BASE + 1.75f, -4.0f }, { 0.5f, 1.75f, 4.0f } },
        };
        nodes = ResourceSpawn::generate(gameData, habitats, worldSeed, world);
        m_home = player.body.position;
        for (const Skill& skill : gameData.skills) if (PlayerActions::parse(skill.animation) == PlayerAction::FISH) m_fishSkill = skill.id;
    }

    // Devuelve true si hay que pausar el juego (ESC fuera del modo lanzamiento).
    bool update(float dt, const InputState& input) {
        bool pause = false;
        if (input.escape) {
            if (aimMode) aimMode = false;
            else pause = true;
        }
        m_clock += dt;
        world.tide = TIDE_AMPLITUDE * std::sin(m_clock * 6.2831853f / TIDE_PERIOD);
        if (input.aimToggle) aimMode = !aimMode;
        m_aiming = aimMode || input.aimHold;
        if (input.ballSwitch != 0) {
            if (m_aiming) inventory.cycle(input.ballSwitch);
            else storage.cycleActive(input.ballSwitch); // fuera del modo captura cambia el pokémon que acompaña
        }
        if (input.modeSwitch) {
            storage.cycleMode();
            showNotice(Notice::REWARD, std::string("Modo: ") + CompanionRules::label(storage.mode));
        }
        if (input.teamSelect > 0) storage.setActive(input.teamSelect - 1); // la misma tecla que el que ya está fuera no cambia nada

        wild.update(dt, player.body.position.x, player.body.position.z, camera.yaw(), *m_data, habitats, [this](CaptureTarget& target) { return assignSpecies(target); });

        for (const CaptureTarget* target : wild.loaded()) { // lo que se ve de cerca queda registrado en la Pokédex
            if (target->hittable() && target->speciesIndex() >= 0 && nearPlayer(target->body.position, SEEN_RANGE) && visible(*target)) storage.markSeen(target->speciesId());
        }

        if (input.lockCancel) lockedTarget = nullptr;
        else if (input.lockTap) cycleLock();
        validateLock(dt);

        camera.setSwimming(player.swimming());
        camera.update(dt, m_aiming);
        if (lockedTarget) camera.trackToward(player.body.position, lockedTarget->center(), dt);
        else camera.rotate(input.lookX, input.lookY);

        // Los pokémon son sólidos para quien los pisa: el jugador los rodea igual que a las paredes.
        world.creatures.clear();
        for (const CaptureTarget* target : wild.loaded()) if (target->hittable()) world.creatures.push_back(target->solid());
        world.props.clear();
        for (const Chest& chest : chests.entities) if (chest.closed()) world.props.push_back(chest.solid());
        for (const ResourceNode& node : nodes) if (nearPlayer(node.body.position, PROP_RANGE)) world.props.push_back(node.solid());
        world.props.push_back(machine.solid());

        player.aiming = m_aiming;
        InputState moveInput = input;
        if (fishing.hooked()) moveInput.crouch = moveInput.crouchHeld = false; // los botones de decidir (Liberar) no agachan
        player.update(dt, moveInput, camera.yaw(), camera.lookAngle(), world);
        updateHealth(dt);

        weather.update(dt);
        throwCooldown = (std::max)(0.0f, throwCooldown - dt);
        noticeTime = (std::max)(0.0f, noticeTime - dt);
        if (m_aiming && input.throwBall && throwCooldown <= 0.0f && m_throwDelay < 0.0f) startThrow();
        if (m_throwDelay >= 0.0f && (m_throwDelay -= dt) < 0.0f) throwBall();

        for (CaptureTarget* target : wild.loaded()) {
            target->watch(camera.yaw());
            const CaptureTarget::Event event = target->update(dt, world);
            if (event == CaptureTarget::Event::ESCAPED && wild.isHooked(target)) fishing.renewHook(); // sigue enganchado
            handleCaptureEvent(*target, event);
        }
        updateBalls(dt);
        chests.update(dt, dayCycle.day(), player.body.position.x, player.body.position.z, *m_data, habitats, world);
        m_concealed = false;
        for (ResourceNode& node : nodes) {
            node.update(dt);
            if (!m_concealed && concealsPlayer(node)) m_concealed = true;
        }
        exploration.reveal(player.body.position.x, player.body.position.z);
        groundItems.update(dt, GroundSpawn::DELAY,
            [&](GroundItem& item) { world.step(item.body, dt); item.update(dt); },
            [&](int spot) { return GroundSpawn::make(*m_data, spot); });
        updateCompanion(dt);
        updateStrikes(dt);
        updateWaterfall(dt);
        updateInteraction();
        m_holdClock = (std::max)(0.0f, m_holdClock - dt);
        const bool hold = input.interactHeld && m_interaction.kind == Interaction::NODE && m_holdClock <= 0.0f; // mantener sigue trabajando el recurso
        const bool fished = updateFishing(dt, input);
        if (!fished && (input.interact || hold)) {
            interact();
            m_holdClock = HOLD_REPEAT;
        }
        updateAimInfo();
        return pause;
    }

    const GameData& data() const { return *m_data; }

    // Golpe reciente del acompañante (para dibujar el rayo de los ataques a distancia y el impacto, con el color de su tipo).
    struct Strike {
        DirectX::XMFLOAT3 from;
        DirectX::XMFLOAT3 to;
        int typeId = -1;
        bool ranged = false;
        float age = 0.0f;
    };
    static constexpr float STRIKE_TIME = 0.3f;
    std::vector<Strike> strikes;

    // Al empezar una partida nueva el jugador elige su pokémon inicial: aún no tiene ninguno.
    bool needsStarter() const { return storage.empty(); }

    // Crea el pokémon inicial: nivel inicial, analizado, con su pokéball exclusiva y sus 3 mejores estadísticas (las de su
    // evolución final) con el EV máximo al máximo.
    void chooseStarter(int speciesId) {
        const PokemonSpecies* species = m_data->speciesById(speciesId);
        const PokeballType* ball = m_data->starterBall();
        if (!species || !species->starter || !needsStarter()) return;

        OwnedPokemon owned;
        owned.speciesId = species->id;
        owned.level = PokemonRules::STARTER_LEVEL;
        owned.ballId = ball ? ball->id : -1;
        owned.analyzed = true;
        owned.evCaps = EvRules::starterCaps(m_data->finalForm(*species)->stats);
        bool sentToPc = false;
        storage.add(owned, sentToPc);
    }

    DirectX::XMMATRIX getViewMatrix() const {
        const DirectX::XMFLOAT3 at = camera.focus(player.body.position), full = camera.eye(player.body.position);
        const float pull = (std::max)(world.clearFraction(at, full, 0.5f), Camera::MIN_PULL); // una pared (galerías) acerca la cámara
        const DirectX::XMFLOAT3 eye = { at.x + (full.x - at.x) * pull, at.y + (full.y - at.y) * pull, at.z + (full.z - at.z) * pull };
        const float feet = player.body.feet();
        const float floor = player.swimming() ? world.groundHeight(eye.x, eye.z, feet) : world.surfaceHeight(eye.x, eye.z, feet);
        const float ceiling = world.under(eye.x, eye.z, feet) ? world.ceiling(eye.x, eye.z) - 0.3f : Physics::World::ABOVE;
        return camera.viewMatrix(player.body.position, floor + 0.35f, ceiling, pull); // la cámara no se hunde en el terreno (bajo el agua sí se sumerge)
    }

    GameStatus status() const {
        GameStatus s;
        s.aimBlend = camera.aimBlend();
        s.playerLevel = progress.level();
        s.playerXp = progress.xpFraction();
        s.levelCap = progress.levelCap();
        s.notice = noticeTime > 0.0f ? notice : Notice::NONE;
        s.locked = lockedTarget != nullptr;
        s.inventory = &inventory;
        s.data = m_data;
        s.aiming = m_aiming;
        s.canLock = anyLockable();
        if (m_missingSkill >= 0) if (const Skill* skill = m_data->skill(m_missingSkill)) {
            s.missingSkill = &skill->name;
            s.missingLevel = m_missingLevel;
            s.missingPos = m_missingPos;
        }
        s.interactPos = interactionAnchor();
        fillLocation(s);
        fillWorldState(s);
        s.habitatMap = &habitats;
        s.exploration = &exploration;
        for (const Physics::World::Box& wall : world.obstacles) s.mapBlocks.push_back({ wall.center.x, wall.center.z, wall.half.x, wall.half.z });
        s.mapBlocks.push_back({ ResearchMachine::POSITION.x, ResearchMachine::POSITION.z, ResearchMachine::WIDTH * 0.5f, ResearchMachine::DEPTH * 0.5f });
        s.playerX = player.body.position.x;
        s.playerZ = player.body.position.z;
        s.playerYaw = camera.yaw();
        s.dayAngle = dayCycle.sunAngle();
        if (storage.active()) s.companionMode = CompanionRules::label(storage.mode);
        addNameTags(s);
        for (size_t i = 0; i < storage.team().size(); ++i) {
            const OwnedPokemon& owned = storage.team()[i];
            if (const PokemonSpecies* species = m_data->speciesById(owned.speciesId)) {
                s.team.push_back({ species, owned.level, owned.shiny, owned.ballId, static_cast<int>(i) == storage.activeIndex() });
            }
        }
        if (!noticeText.empty()) s.noticeText = &noticeText;
        if (m_interaction.kind == Interaction::WEAKENED && m_weak) {
            const PokemonSpecies* species = m_data->speciesById(m_weak->speciesId());
            s.interactVerb = CAPTURE_VERB;
            s.interactTarget = species ? &species->name : &GROUND_NAME;
        } else if (m_interaction.kind == Interaction::CHEST) {
            s.interactVerb = CHEST_VERB;
            s.interactTarget = &m_data->chests[chests.entities[m_interaction.index].typeIndex()].name;
        } else if (m_interaction.kind == Interaction::NODE) {
            const ResourceNode& node = nodes[m_interaction.index];
            const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
            const Skill* skill = node.growing() ? m_data->skill(type.skillId) : nullptr;
            s.interactVerb = skill ? skill->name.c_str() : type.action.c_str();
            s.interactTarget = &type.name;
            const Material* material = m_data->material(type.materialId);
            if (!skill && material && type.action == PICK_VERB) s.interactTarget = &material->name; // "Recoger Baya"; los árboles siguen en "Talar Roble"
        } else if (m_interaction.kind == Interaction::GROUND) {
            s.interactVerb = PICK_VERB;
            s.interactTarget = &GROUND_NAME;
            const GroundItem& item = groundItems.entities[m_interaction.index];
            if (const GroundItemType* reward = item.reward() >= 0 ? &m_data->groundItems[item.reward()] : nullptr) {
                if (const Item* found = m_data->item(reward->itemId)) s.interactTarget = &m_data->itemName(*found);
            }
        } else if (m_interaction.kind == Interaction::MACHINE) {
            s.interactVerb = USE_VERB;
            s.interactTarget = &MACHINE_NAME;
        }
        if (m_aimTarget) {
            const CaptureTarget& t = *m_aimTarget;
            s.hasAimTarget = true;
            s.behind = isBehind(t);
            s.hidden = m_concealed;
            s.chancePercent = static_cast<int>(std::lround(chancePercent(t, inventory.captureMultiplier(t.shiny()))));
        }
        return s;
    }

    private:
    // "Pradera · Arcoíris": hábitat que más pesa donde está el jugador y el clima más fuerte.
    void fillLocation(GameStatus& s) const {
        const WildSpawn::Context context = spawnContext(player.body.position.x, player.body.position.z, 0, player.body.underground);
        if (context.habitats.empty()) return;
        const int habitat = static_cast<int>(std::max_element(context.habitats.begin(), context.habitats.end()) - context.habitats.begin());
        const Weather* current = context.weather.empty() ? nullptr : m_data->weather(context.weather.front().weatherId);
        s.habitatName = &m_data->habitats[habitat].name;
        s.weatherName = current ? &current->name : nullptr;
    }

    // Temperatura, altura, aire, región y ayudas inferiores.
    void fillWorldState(GameStatus& s) const {
        const DirectX::XMFLOAT3& p = player.body.position;
        const HabitatMap::Region* region = habitats.regionAt(p.x, p.z);
        s.temperature = temperatureAt(p.x, p.z);
        s.altitude = (std::max)(0.0f, p.y);
        s.health = health.fraction();
        s.hurt = health.hurtFlash();
        s.oxygen = player.oxygen();
        s.exhausted = player.exhausted();
        s.showOxygen = player.diving() || player.exhausted() || !player.oxygenFull();
        const DirectX::XMFLOAT3 eye = camera.eye(p);
        s.underwater = player.diving() && eye.y < world.waterLevel(eye.x, eye.z, player.body.feet());
        s.darkness = habitats.darkness(p.x, p.z, player.body.feet());
        s.layer = static_cast<int>(habitats.layerAt(p.x, p.z, player.body.feet()));
        s.climbing = player.climbing();
        s.swimming = player.swimming();
        s.diving = player.diving();
        if (player.body.underground) {
            if (const Underground::System* system = habitats.systemAt(p.x, p.z)) s.regionName = &m_data->habitats[system->habitat].name;
        } else if (region) {
            s.regionName = &m_data->habitats[region->habitat].name;
        }
        s.fishing = fishing.active();
        s.bobber = fishing.bobber();
        s.biteAlert = fishing.phase() == Fishing::Phase::BITING ? 1.0f - fishing.biteProgress() : 0.0f;
        fillPrompts(s);
    }

    // °C donde está el jugador: las regiones tienen su clima y fuera baja con la altura.
    float temperatureAt(float x, float z) const {
        if (player.body.underground) {
            const Underground::System* system = habitats.systemAt(x, z);
            return system && system->kind == Underground::Kind::MINE ? MINE_TEMPERATURE : CAVE_TEMPERATURE;
        }
        if (const HabitatMap::Region* region = habitats.regionAt(x, z)) {
            switch (region->kind) {
                default: return SKY_TEMPERATURE;
            }
        }
        return HabitatMap::temperature(world.groundHeight(x, z));
    }

    // Ayudas inferiores: la pesca (lanzar, recoger, Capturar / Liberar) y el buceo.
    void fillPrompts(GameStatus& s) const {
        using Phase = Fishing::Phase;
        const auto add = [&](PromptAction action, std::string text, bool enabled = true) { s.prompts.push_back({ action, std::move(text), enabled }); };
        switch (fishing.phase()) {
            case Phase::CASTING:
            case Phase::WAITING: add(PromptAction::INTERACT, "Recoger la caña"); break;
            case Phase::BITING:  add(PromptAction::INTERACT, "¡Pica! Recoger"); break;
            case Phase::HOOKED:
                if (hookReady()) {
                    add(PromptAction::INTERACT, inventory.canThrow() ? "Capturar con " + inventory.selectedBall().type.name : "Capturar (sin pokéballs)", inventory.canThrow());
                    add(PromptAction::RELEASE, "Liberar");
                }
                break;
            default:
                if (m_castHint) add(PromptAction::INTERACT, "Pescar");
                break;
        }
    }

    // Nombre y nivel sobre cada pokémon visible (los salvajes y el acompañante).
    void addNameTags(GameStatus& s) const {
        constexpr float TAG_RANGE = 30.0f;
        constexpr float TAG_LIFT = 0.35f;
        const auto add = [&](int speciesId, int level, bool shiny, const DirectX::XMFLOAT3& head, float hp = 1.0f) {
            const PokemonSpecies* species = m_data->speciesById(speciesId);
            const float dx = head.x - player.body.position.x, dz = head.z - player.body.position.z;
            if (species && dx * dx + dz * dz <= TAG_RANGE * TAG_RANGE) {
                s.nameTags.push_back({ { head.x, head.y + TAG_LIFT, head.z }, (shiny ? "* " : "") + species->name + "  Nv. " + std::to_string(level) +
                    (hp < 1.0f ? "  PS " + std::to_string(static_cast<int>(std::ceil(hp * 100.0f))) + "%" : std::string()), shiny });
            }
        };
        for (const CaptureTarget* target : wild.loaded()) {
            if (target->hittable()) add(target->speciesId(), target->level(), target->shiny(), { target->body.position.x, target->body.position.y + CaptureTarget::SIZE, target->body.position.z }, target->hpFraction());
        }
        if (companion.active() && !companion.changing() && storage.active()) {
            const OwnedPokemon& lead = *storage.active();
            add(companion.speciesId(), lead.level, lead.shiny, { companion.body.position.x, companion.body.position.y + Companion::SIZE, companion.body.position.z });
        }
    }

    bool nearPlayer(const DirectX::XMFLOAT3& at, float range) const {
        const float dx = at.x - player.body.position.x, dz = at.z - player.body.position.z;
        return dx * dx + dz * dz <= range * range;
    }

    // Punto del mundo sobre lo que se interactúa, donde se dibuja la ayuda de cómo hacerlo.
    DirectX::XMFLOAT3 interactionAnchor() const {
        const auto above = [](const Physics::Body& body, float height) { return DirectX::XMFLOAT3{ body.position.x, body.position.y + height, body.position.z }; };
        switch (m_interaction.kind) {
            case Interaction::CHEST:   return above(chests.entities[m_interaction.index].body, Chest::HEIGHT + 0.5f);
            case Interaction::NODE:    return nodeAnchor(nodes[m_interaction.index]);
            case Interaction::GROUND:  return above(groundItems.entities[m_interaction.index].body, 0.9f);
            case Interaction::MACHINE: return above(machine.body, ResearchMachine::HEIGHT + 0.5f);
            case Interaction::WEAKENED: return m_weak ? DirectX::XMFLOAT3{ m_weak->body.position.x, m_weak->body.position.y + CaptureTarget::SIZE + 0.9f, m_weak->body.position.z } : DirectX::XMFLOAT3{};
            default:                   return {};
        }
    }

    static DirectX::XMFLOAT3 nodeAnchor(const ResourceNode& node) {
        const float height = (std::min)(ResourceStyle::look(node.typeId()).height, 1.6f) + 0.5f; // los árboles altos: a mano del jugador
        return { node.body.position.x, node.body.position.y + height, node.body.position.z };
    }

    // --- Porcentaje de captura ---
    // ¿Oculta este nodo al jugador? Solo los matorrales y la hierba alta (forma BUSH) y (con o sin frutos).
    // Daño por caída (física: la altura equivalente a la velocidad de impacto) y por ahogarse; si la vida llega a 0 vuelve al inicio.
    void updateHealth(float dt) {
        health.update(dt);
        health.hurt(HealthRules::fallDamage(player.takeImpact()));
        if (player.drowning()) health.hurt(HealthRules::DROWN_PER_SECOND * dt);
        if (!health.dead()) return;
        player.body.position = m_home;
        player.body.velocity = { 0.0f, 0.0f, 0.0f };
        health.restore();
        showNotice(Notice::FAINTED);
    }

    bool concealsPlayer(const ResourceNode& node) const {
        if (ResourceStyle::look(node.typeId()).shape != ResourceStyle::Shape::BUSH) return false;
        const float dx = node.body.position.x - player.body.position.x, dz = node.body.position.z - player.body.position.z;
        return dx * dx + dz * dz <= CONCEAL_RADIUS * CONCEAL_RADIUS && std::fabs(node.body.position.y - player.body.position.y) < 1.5f;
    }

    bool isBehind(const CaptureTarget& t) const {
        return CaptureRules::isBehind(t.body.position, t.yaw(), player.body.position);
    }

    float chancePercent(const CaptureTarget& t, float ballMultiplier) const {
        const PokemonSpecies* species = m_data->speciesById(t.speciesId());
        const float base = species ? CaptureRules::basePercent(species->catchRate, t.level(), progress.level()) * CaptureRules::hpFactor(t.hpFraction()) : 0.0f;
        return CaptureRules::percent(base, isBehind(t), m_concealed, ballMultiplier);
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

        std::string text;
        float rarityXp = 1.0f; // la rareza de la especie multiplica la experiencia que da capturarla
        if (const PokemonSpecies* species = m_data->speciesById(target.speciesId())) {
            rarityXp = m_data->xpMultiplier(*species);
            OwnedPokemon owned;
            owned.speciesId = species->id;
            owned.level = (std::min)(target.level(), progress.levelCap()); // no puede pasar del límite del jugador
            owned.shiny = target.shiny();
            owned.ballId = target.ballId();
            owned.evCaps = EvRules::rollCaps(species->stats, CaptureRules::minimumRank(target.throwKind()));
            bool sentToPc = false;
            const bool stored = storage.add(owned, sentToPc);
            text = (owned.shiny ? "* " : "") + species->name + "  Nv. " + std::to_string(owned.level) +
                   (!stored ? " (sin espacio)" : sentToPc ? " (enviado al PC)" : "");
        }
        if (const int levels = progress.addXp(static_cast<int>(std::lround(static_cast<float>(Xp::CAPTURE) * rarityXp))); levels > 0) text += "   ¡Nivel " + std::to_string(progress.level()) + "!";
        switch (target.throwKind()) {
            case CaptureRules::Throw::LUCKY:       showNotice(Notice::LUCKY, text); break;
            case CaptureRules::Throw::SUPER_LUCKY: showNotice(Notice::SUPER_LUCKY, text); break;
            default:                               showNotice(Notice::CAPTURED, text); break;
        }
    }

    // Hábitat, clima y hora de un punto del mundo: lo que decide qué pokémon aparecen ahí.
    WildSpawn::Context spawnContext(float x, float z, int rod = 0, bool underground = false) const {
        WildSpawn::Context context;
        habitats.weights(x, z, context.habitats, underground);
        context.weather = weather.at(context.habitats);
        context.night = dayCycle.isNight();
        context.depth = underground ? habitats.under().depthAt(x, z) : world.waterDepth(x, z);
        context.rod = rod;
        return context;
    }

    // Especie de un pokémon salvaje que acaba de aparecer, según el hábitat, el clima y la hora de donde sale.
    // false si ahí no puede vivir ninguno (el sitio se descarta).
    bool assignSpecies(CaptureTarget& target) const {
        const int index = WildSpawn::pick(*m_data, spawnContext(target.body.position.x, target.body.position.z, 0, target.deepSlot()));
        if (index < 0) return false;
        target.setSpecies(index, m_data->species[index].id, PokemonRules::rollLevel(m_data->species[index]), PokemonRules::rollShiny());
        target.setSwims(m_data->species[index].swims);
        return true;
    }

    // Pokémon que pica: especie según el agua de donde está el corcho y el nivel de la caña.
    bool assignFished(CaptureTarget& target) const {
        const int index = WildSpawn::pick(*m_data, spawnContext(target.body.position.x, target.body.position.z, rodLevel()));
        if (index < 0) return false;
        target.setSpecies(index, m_data->species[index].id, PokemonRules::rollLevel(m_data->species[index]), PokemonRules::rollShiny());
        target.setSwims(true);
        return true;
    }

    // --- Pesca ---
    int rodLevel() const { return m_fishSkill >= 0 ? inventory.skillLevel(m_fishSkill) : 0; }
    bool hookReady() const { return wild.hookActive() && wild.hooked().hittable() && !wild.hooked().leaving(); }

    // Fin de la pesca (recoge la línea y, si hay pokémon enganchado, lo suelta).
    void endFishing(bool releaseHooked) {
        if (releaseHooked && wild.hookActive()) wild.releaseHook(player.body.position.x, player.body.position.z);
        fishing.reset();
        player.hold(PlayerAction::NONE);
    }

    // Capturar al enganchado: la pokéball sale de la mano hacia él (como un lanzamiento normal).
    void captureHooked() { captureDirect(wild.hooked()); }

    // Lanza la pokéball seleccionada directamente a un pokémon (el enganchado o el debilitado por el acompañante).
    void captureDirect(const CaptureTarget& wildTarget) {
        if (!inventory.canThrow()) {
            showNotice(Notice::OUT_OF_STOCK);
            return;
        }
        const DirectX::XMFLOAT3 target = wildTarget.center();
        const DirectX::XMFLOAT3 hand = handPosition();
        DirectX::XMFLOAT3 dir = { target.x - hand.x, target.y - hand.y, target.z - hand.z };
        const float length = (std::max)(std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z), 1.0e-3f);
        dir = { dir.x / length, dir.y / length, dir.z / length };
        player.act(PlayerAction::THROW);
        launch(target, dir);
    }

    // Entrada y eventos de la pesca. true si la pulsación de interactuar se ha usado aquí.
    bool updateFishing(float dt, const InputState& input) {
        using Event = Fishing::Event;
        bool used = false, reel = false;
        const float yaw = camera.yaw();
        const DirectX::XMFLOAT3& p = player.body.position;
        const bool standing = !player.swimming() && !player.climbing() && player.body.onGround;
        DirectX::XMFLOAT3 water;
        m_castHint = !fishing.active() && rodLevel() > 0 && standing && m_interaction.kind == Interaction::NONE && Fishing::findWater(world, p.x, p.z, yaw, water, player.body.feet());

        if (fishing.active() && rodLevel() <= 0) endFishing(true);
        if (fishing.active()) {
            if (input.interact) {
                used = true;
                if (fishing.hooked()) { if (hookReady()) captureHooked(); }
                else reel = true;
            }
            if (input.release && fishing.hooked() && hookReady()) endFishing(true);
        } else if (input.interact && m_castHint) {
            used = fishing.cast(world, p.x, p.z, yaw, inventory.skillSpeed(m_fishSkill), player.body.feet());
            if (used) {
                player.act(PlayerAction::FISH);
                player.hold(PlayerAction::FISH);
            }
        }

        switch (fishing.update(dt, reel, world, p.x, p.z)) {
            case Event::HOOKED:
                if (!wild.hook(fishing.bobber().x, fishing.bobber().y, fishing.bobber().z, yaw, [this](CaptureTarget& target) { return assignFished(target); })) {
                    endFishing(false);
                    showNotice(Notice::ESCAPED);
                }
                break;
            case Event::MISSED:   showNotice(Notice::ESCAPED); endFishing(false); break;
            case Event::TIMEOUT:
            case Event::LINE_CUT: endFishing(true); break;
            default: break;
        }
        if (fishing.hooked() && !wild.hookActive()) endFishing(false); // lo capturó o se fue
        if (!fishing.active() && !wild.hookActive()) player.hold(PlayerAction::NONE);
        return used;
    }

    // Un pokémon que evoluciona al remontar una cascada (Magikarp) la remonta junto al jugador: pasado un rato en ella, evoluciona.
    void updateWaterfall(float dt) {
        const OwnedPokemon* lead = storage.active();
        const PokemonSpecies* species = lead ? m_data->speciesById(lead->speciesId) : nullptr;
        bool climbs = false;
        if (species) for (const Evolution& e : species->evolutions) climbs = climbs || e.method == Evolution::Method::WATERFALL;
        const DirectX::XMFLOAT3& p = player.body.position;
        const bool inFall = climbs && player.swimming() && habitats.nearCascade(p.x, p.z, CASCADE_RANGE);
        m_cascadeTime = inFall ? m_cascadeTime + dt : (std::max)(0.0f, m_cascadeTime - dt);
        if (m_cascadeTime < CASCADE_TIME) return;
        m_cascadeTime = 0.0f;
        const std::string before = species->name;
        if (const PokemonSpecies* next = storage.evolveBy(storage.activeIndex(), Evolution::Method::WATERFALL)) {
            showNotice(Notice::REWARD, "¡" + before + " ha evolucionado a " + next->name + "!");
        }
    }

    // El líder del equipo acompaña al jugador y trabaja solo lo cercano: recoge plantas crecidas y objetos sueltos (cualquier
    // pokémon) y trabaja los nodos y riega las plantas si su habilidad del mundo llega al nivel que piden.
    void updateCompanion(float dt) {
        const OwnedPokemon* lead = storage.active();
        const PokemonSpecies* species = lead ? m_data->speciesById(lead->speciesId) : nullptr;
        if (!lead && companion.active()) companion.set(-1, false, player.body.position, camera.yaw());
        else if (lead && species && (lead->uid != m_shownUid || lead->speciesId != m_shownSpecies)) {
            m_shownUid = lead->uid; // otro pokémon (o ha evolucionado): vuelve el que está fuera y sale su pokéball
            m_shownSpecies = lead->speciesId;
            companion.swap(species->id, m_data->levitates(*species), lead->ballId, player.body.position, camera.yaw());
        }
        if (companion.changing()) species = nullptr; // durante el cambio no trabaja

        // Busca lo más cercano que sabe trabajar dentro de su radio de búsqueda; si está lejos, va hacia ello (aunque se separe
        // del jugador) y, al llegar, lo trabaja. Sin nada que hacer vuelve a seguir al jugador.
        ResourceNode* nodeTarget = nullptr;
        GroundItem* groundTarget = nullptr;
        int level = 1;
        float best = 1.0e9f;
        const auto distance2 = [&](const DirectX::XMFLOAT3& at) {
            const float dx = at.x - companion.body.position.x, dz = at.z - companion.body.position.z;
            return dx * dx + dz * dz;
        };
        const auto inRange = [&](const DirectX::XMFLOAT3& at) {
            const float dx = at.x - player.body.position.x, dz = at.z - player.body.position.z;
            return dx * dx + dz * dz <= Companion::SEARCH_RANGE * Companion::SEARCH_RANGE;
        };
        const CompanionRules::Mode mode = storage.mode;
        tickCollectWindow(dt);
        if (species && mode == CompanionRules::Mode::COLLECT && m_collecting) {
            for (ResourceNode& node : nodes) {
                if (node.depleted() || !inRange(node.body.position)) continue;
                const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
                const int skillLevel = species->levelIn(type.skillId);
                if (node.growing() || !node.plant()) {
                    if (skillLevel < type.level) continue; // no sabe regar / extraer esto, o no llega al nivel
                }
                const float d2 = distance2(node.body.position);
                if (d2 < best) {
                    best = d2;
                    nodeTarget = &node;
                    groundTarget = nullptr;
                    level = (std::max)(1, skillLevel);
                }
            }
            for (GroundItem& item : groundItems.entities) {
                if (!item.available() || !inRange(item.body.position)) continue;
                const float d2 = distance2(item.body.position);
                if (d2 < best) {
                    best = d2;
                    groundTarget = &item;
                    nodeTarget = nullptr;
                    level = 1;
                }
            }
        }
        CaptureTarget* fightTarget = species && mode != CompanionRules::Mode::COLLECT ? fightTargetFor(mode) : nullptr;
        if (fightTarget) best = distance2(fightTarget->body.position);
        const DirectX::XMFLOAT3* goal = nodeTarget ? &nodeTarget->body.position : groundTarget ? &groundTarget->body.position :
                                        fightTarget ? &fightTarget->body.position : nullptr;
        const bool ranged = species && DamageRules::styleOf(*species) == DamageRules::Style::RANGED;
        const float fightReach = ranged ? CompanionRules::RANGED_RANGE : CompanionRules::ATTACK_RANGE;
        const float reach = nodeTarget ? Companion::WORK_RANGE + ResourceStyle::look(nodeTarget->typeId()).half :
                            fightTarget ? fightReach : Companion::WORK_RANGE;
        const float approach = fightTarget && ranged ? fightReach * CompanionRules::RANGED_APPROACH : Companion::APPROACH;
        bool arrived = goal && best <= reach * reach;
        if (arrived && fightTarget && ranged) arrived = !world.blocked(companion.center(), fightTarget->center()); // sin roca de por medio
        if (arrived && fightTarget && !ranged) arrived = best <= fightReach * fightReach;
        if (nodeTarget && arrived) nodeTarget->companionWorking();
        const float teamBonus = nodeTarget && arrived ? nodeTarget->teamBonus(false) : 1.0f;
        const float power = arrived ? Companion::workPower(fightTarget ? 1 : level, teamBonus) : 0.0f;
        if (!companion.update(dt, world, player.body.position, camera.yaw(), power, goal, approach)) return;
        if (nodeTarget) workNode(*nodeTarget, false);
        else if (groundTarget) collect(*groundTarget);
        else if (fightTarget) strike(*fightTarget);
    }

    // En modo recolección el acompañante solo recoge un rato cada cierto tiempo; el resto sigue al jugador.
    void tickCollectWindow(float dt) {
        m_collectClock -= dt;
        if (m_collectClock > 0.0f) return;
        m_collecting = !m_collecting;
        m_collectClock = m_collecting ? COLLECT_WINDOW : RandomUtil::range(COLLECT_WAIT_MIN, COLLECT_WAIT_MAX);
    }

    // El salvaje más cercano al jugador (dentro del radio de búsqueda) al que el acompañante debe atacar según su modo: en captura
    // solo mientras le quede más vida que el límite; en combate, cualquiera.
    CaptureTarget* fightTargetFor(CompanionRules::Mode mode) {
        CaptureTarget* best = nullptr;
        float bestDistance = Companion::SEARCH_RANGE * Companion::SEARCH_RANGE;
        for (CaptureTarget* target : wild.loaded()) {
            if (!target->hittable() || target->speciesIndex() < 0 || wild.isHooked(target) || target->body.underground != player.body.underground) continue; // el pescado es del jugador
            if (mode == CompanionRules::Mode::CAPTURE && target->hpFraction() <= CompanionRules::CAPTURE_STOP_HP) continue;
            if (!hitOn(*target).lands()) continue; // inmune a sus tipos: no lo ataca
            const float dx = target->body.position.x - player.body.position.x, dz = target->body.position.z - player.body.position.z;
            if (dx * dx + dz * dz < bestDistance) {
                bestDistance = dx * dx + dz * dz;
                best = target;
            }
        }
        return best;
    }

    // Lo que el líder del equipo le haría a un salvaje de un golpe (DamageRules); sin líder o sin datos, nada.
    DamageRules::Hit hitOn(const CaptureTarget& target) const {
        const OwnedPokemon* lead = storage.active();
        const PokemonSpecies* attacker = lead ? m_data->speciesById(lead->speciesId) : nullptr;
        const PokemonSpecies* defender = m_data->speciesById(target.speciesId());
        if (!attacker || !defender) return {};
        return DamageRules::compute(*m_data, *attacker, lead->level, *defender, target.level());
    }

    void updateStrikes(float dt) {
        for (Strike& strike : strikes) strike.age += dt;
        strikes.erase(std::remove_if(strikes.begin(), strikes.end(), [](const Strike& s) { return s.age >= STRIKE_TIME; }), strikes.end());
    }

    // Un golpe del acompañante. Si derrota al salvaje, este gana experiencia (y puede subir de nivel o evolucionar).
    void strike(CaptureTarget& target) {
        const OwnedPokemon* lead = storage.active();
        const PokemonSpecies* attacker = lead ? m_data->speciesById(lead->speciesId) : nullptr;
        if (!attacker) return;

        const DamageRules::Hit hit = hitOn(target);
        if (!hit.lands()) return;
        strikes.push_back({ companion.center(), target.center(), hit.typeId, hit.style == DamageRules::Style::RANGED, 0.0f });
        const std::string name = attacker->name;
        if (!target.damage(hit.fraction)) return;

        const int xp = PokemonRules::battleXp(target.level());
        const PokemonStorage::XpResult result = storage.addXp(storage.activeIndex(), xp, progress.levelCap());
        std::string text = name + "  +" + std::to_string(xp) + " PX";
        if (result.levels > 0) text += "   ¡Nivel " + std::to_string(storage.active()->level) + "!";
        showNotice(Notice::REWARD, std::move(text));
    }

    // --- Interacción (cofres y recursos) ---
    // Lo interactuable más cercano de la lista que esté al alcance del jugador: devuelve su índice y deja la distancia² en best.
    template <typename Entity, typename Usable>
    int nearestIn(const std::vector<Entity>& list, Usable usable, float& best) const {
        int index = -1;
        for (size_t i = 0; i < list.size(); ++i) {
            if (!usable(list[i]) || list[i].body.underground != player.body.underground) continue; // bajo tierra y en la superficie no se alcanzan
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

    // ¿Puede el jugador trabajar este nodo ahora? Regar una planta o extraer exigen su herramienta (nivel mínimo);
    // recoger una planta crecida no exige nada.
    bool playerCanWork(const ResourceNode& node) const {
        const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
        return (node.plant() && !node.growing()) || inventory.skillLevel(type.skillId) >= type.level;
    }

    void updateInteraction() {
        float best = 1.0e9f;
        m_interaction = {};
        m_missingSkill = -1;
        m_weak = weakenedTarget();
        if (const int i = nearestIn(chests.entities, [](const Chest& c) { return c.closed(); }, best); i >= 0) m_interaction = { Interaction::CHEST, i };
        if (const int i = nearestIn(groundItems.entities, [](const GroundItem& g) { return g.available(); }, best); i >= 0) m_interaction = { Interaction::GROUND, i };
        const float mx = machine.body.position.x - player.body.position.x, mz = machine.body.position.z - player.body.position.z;
        if (const float d2 = mx * mx + mz * mz; d2 <= machine.reach() * machine.reach() && d2 < best) {
            best = d2;
            m_interaction = { Interaction::MACHINE, 0 };
        }
        if (const int i = nearestIn(nodes, [](const ResourceNode& n) { return !n.depleted(); }, best); i >= 0) {
            const ResourceNode& node = nodes[i];
            if (playerCanWork(node)) m_interaction = { Interaction::NODE, i };
            else {
                const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
                m_missingSkill = type.skillId; // sin herramienta de ese nivel: solo un pokémon que llegue al nivel puede trabajarlo
                m_missingLevel = type.level;
                m_missingPos = nodeAnchor(node);
            }
        }
        if (m_weak) m_interaction = { Interaction::WEAKENED, 0 }; // capturar al debilitado tiene prioridad
    }

    // En modo captura, el salvaje más cercano que el acompañante ha dejado debilitado (con la vida en o bajo CAPTURE_STOP_HP) y que se
    // puede capturar ya: aparece la ayuda de interacción "Capturar" sobre él.
    const CaptureTarget* weakenedTarget() const {
        if (storage.mode != CompanionRules::Mode::CAPTURE || !storage.active() || fishing.active() || m_aiming) return nullptr;
        const CaptureTarget* best = nullptr;
        float bestDistance = CompanionRules::WEAK_REACH * CompanionRules::WEAK_REACH;
        for (const CaptureTarget* target : wild.loaded()) {
            if (!target->hittable() || target->speciesIndex() < 0 || wild.isHooked(target) || target->body.underground != player.body.underground) continue;
            if (target->hpFraction() > CompanionRules::CAPTURE_STOP_HP) continue;
            const float dx = target->body.position.x - player.body.position.x, dz = target->body.position.z - player.body.position.z;
            if (dx * dx + dz * dz < bestDistance) {
                bestDistance = dx * dx + dz * dz;
                best = target;
            }
        }
        return best;
    }

    void interact() {
        if (m_interaction.kind == Interaction::WEAKENED && m_weak) {
            captureDirect(*m_weak);
            return;
        }
        if (m_interaction.kind == Interaction::CHEST) {
            player.act(PlayerAction::OPEN);
            openChest(chests.entities[m_interaction.index]);
        } else if (m_interaction.kind == Interaction::GROUND) {
            player.act(PlayerAction::PICK);
            collect(groundItems.entities[m_interaction.index]);
        } else if (m_interaction.kind == Interaction::MACHINE) researchRequested = true;
        else if (m_interaction.kind == Interaction::NODE) {
            ResourceNode& node = nodes[m_interaction.index];
            const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
            const bool tool = node.growing() || !node.plant();
            const Skill* skill = tool ? m_data->skill(type.skillId) : nullptr;
            player.act(skill ? PlayerActions::parse(skill->animation) : PlayerAction::PICK); // la animación de su herramienta; recoger una planta: agacharse
            workNode(node, true, tool ? inventory.skillSpeed(type.skillId) : 1.0f);
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
        if (reward) giveReward(reward->itemId, RandomUtil::integer(reward->minQuantity, reward->maxQuantity));
        gainXp(Xp::CHEST);
    }

    // Un paso de trabajo en el nodo (riego o golpe); al agotarlo da su material.
    void workNode(ResourceNode& node, bool byPlayer, float speed = 1.0f) {
        if (!node.work(byPlayer, speed) || !node.takeHarvest()) return;
        const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
        if (const Item* item = m_data->materialItem(type.materialId)) giveReward(item->id, RandomUtil::integer(type.minYield, type.maxYield));
        gainXp(Xp::GATHER);
    }

    // Recoge un objeto suelto del mundo.
    void collect(GroundItem& item) {
        if (!item.available()) return;
        if (item.reward() >= 0) {
            const GroundItemType* reward = &m_data->groundItems[item.reward()];
            giveReward(reward->itemId, RandomUtil::integer(reward->minQuantity, reward->maxQuantity));
            gainXp(Xp::PICK_UP);
        }
        item.take();
    }

    // Experiencia del jugador por una acción en el mundo. Si sube de nivel lo avisa (salvo que ya haya un aviso de recompensa).
    void gainXp(int amount) {
        if (progress.addXp(amount) <= 0) return;
        const std::string level = "Nivel " + std::to_string(progress.level());
        if (noticeTime > 0.0f && notice == Notice::REWARD) noticeText += "   ¡" + level + "!"; // no tapa el aviso de la recompensa
        else showNotice(Notice::LEVEL_UP, level);
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

    // ¿Se ve el pokémon desde el jugador, sin paredes, rocas, árboles ni cofres de por medio?
    bool visible(const CaptureTarget& t) const {
        const DirectX::XMFLOAT3 chest = { player.body.position.x, player.body.position.y + CHEST_HEIGHT, player.body.position.z };
        return !world.blocked(chest, t.center());
    }

    // Objetivos disponibles para fijar (vivos, dentro del alcance y a la vista).
    std::vector<CaptureTarget*> lockCandidates(const CaptureTarget* exclude) const {
        std::vector<CaptureTarget*> result;
        for (CaptureTarget* target : wild.loaded()) {
            if (target != exclude && target->hittable() && distanceXZ(*target) <= RANGE && visible(*target)) result.push_back(target);
        }
        return result;
    }

    CaptureTarget* closestToCenter(const std::vector<CaptureTarget*>& candidates) const {
        return *std::min_element(candidates.begin(), candidates.end(),
            [this](const CaptureTarget* a, const CaptureTarget* b) { return std::fabs(relativeAngle(*a)) < std::fabs(relativeAngle(*b)); });
    }

    // Una pulsación: fija el objetivo más cercano al centro; si ya hay uno, pasa al siguiente (hacia la derecha).
    void cycleLock() {
        std::vector<CaptureTarget*> candidates = lockCandidates(nullptr);
        if (candidates.empty()) return;
        if (!lockedTarget) {
            lockedTarget = closestToCenter(candidates);
            return;
        }

        std::sort(candidates.begin(), candidates.end(),
            [this](const CaptureTarget* a, const CaptureTarget* b) { return relativeAngle(*a) < relativeAngle(*b); });
        const auto it = std::find(candidates.begin(), candidates.end(), lockedTarget);
        if (it == candidates.end()) lockedTarget = closestToCenter(candidates);
        else lockedTarget = (it + 1 == candidates.end()) ? candidates.front() : *(it + 1);
    }

    // Si el objetivo fijado se captura, queda fuera de alcance o lleva un rato tapado, salta a otro visible; si no hay, se suelta.
    void validateLock(float dt) {
        if (!lockedTarget) return;
        const CaptureTarget& t = *lockedTarget;
        if (t.hittable() && distanceXZ(t) <= RANGE + LOCK_RELEASE_MARGIN) {
            m_lockHidden = visible(t) ? 0.0f : m_lockHidden + dt;
            if (m_lockHidden < LOCK_HIDDEN_GRACE) return;
        }
        m_lockHidden = 0.0f;

        const std::vector<CaptureTarget*> candidates = lockCandidates(lockedTarget);
        lockedTarget = candidates.empty() ? nullptr : closestToCenter(candidates);
    }

    // --- Apuntado ---
    // Pokémon más cercano que atraviesa el rayo (nullptr si ninguno) y la distancia al impacto.
    CaptureTarget* nearestHit(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& dir, float& distance, bool inRangeOnly) const {
        CaptureTarget* best = nullptr;
        distance = 1.0e9f;
        for (CaptureTarget* target : wild.loaded()) {
            float t = 0.0f;
            if (inRangeOnly && distanceXZ(*target) > RANGE) continue;
            if (world.blocked(eye, target->center())) continue; // tapado por un obstáculo
            if (target->raycast(eye, dir, t) && t < distance) {
                distance = t;
                best = target;
            }
        }
        return best;
    }

    // Pokémon al que apunta ahora la cruceta (para mostrar su porcentaje).
    void updateAimInfo() {
        m_aimTarget = nullptr;
        if (!m_aiming) return;
        float distance;
        m_aimTarget = nearestHit(camera.eye(player.body.position), camera.forward(), distance, true);
    }

    // Pulsar lanzar: empieza la animación; la bola sale de la mano un instante después (throwBall).
    void startThrow() {
        throwCooldown = THROW_COOLDOWN;
        if (!inventory.canThrow()) {
            showNotice(Notice::OUT_OF_STOCK);
            return;
        }
        m_throwDelay = THROW_DELAY;
        player.act(PlayerAction::THROW);
    }

    // Distancia a la que el rayo toca el relieve o la superficie del agua (maxDistance si no toca nada).
    float rayToSurface(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& dir, float maxDistance) const {
        const auto below = [&](float t) {
            const float x = eye.x + dir.x * t, y = eye.y + dir.y * t, z = eye.z + dir.z * t;
            return y <= world.surfaceHeight(x, z, y) || world.solid(x, y, z);
        };
        float previous = 0.0f;
        for (float t = AIM_STEP; t <= maxDistance; t += AIM_STEP) {
            if (below(t)) {
                float low = previous, high = t;
                for (int i = 0; i < 6; ++i) { // afina el punto de impacto
                    const float mid = (low + high) * 0.5f;
                    (below(mid) ? high : low) = mid;
                }
                return high;
            }
            previous = t;
        }
        return maxDistance;
    }

    // La mano derecha del personaje: de donde sale la pokéball.
    DirectX::XMFLOAT3 handPosition() const {
        float sinYaw, cosYaw;
        DirectX::XMScalarSinCos(&sinYaw, &cosYaw, player.facing());
        const DirectX::XMFLOAT3& p = player.body.position;
        return { p.x + cosYaw * SPAWN_SIDE + sinYaw * SPAWN_FORWARD,
                 p.y + SPAWN_HEIGHT * player.heightScale(),
                 p.z - sinYaw * SPAWN_SIDE + cosYaw * SPAWN_FORWARD };
    }

    // Lanza la pokéball equipada hacia el punto que señala la cruceta (centro de la pantalla).
    void throwBall() {
        m_throwDelay = -1.0f;
        if (!inventory.canThrow()) return;
        const DirectX::XMFLOAT3 eye = camera.eye(player.body.position);
        const DirectX::XMFLOAT3 dir = camera.forward();

        // Punto apuntado: primer impacto del rayo de la cámara con el relieve, el agua o un pokémon.
        float distance = rayToSurface(eye, dir, MAX_AIM_DISTANCE);
        float hit = 0.0f;
        if (nearestHit(eye, dir, hit, false)) distance = (std::min)(distance, hit);
        launch({ eye.x + dir.x * distance, eye.y + dir.y * distance, eye.z + dir.z * distance }, dir);
    }

    // Sale una pokéball de la mano del jugador hacia 'aim'.
    void launch(const DirectX::XMFLOAT3& aim, const DirectX::XMFLOAT3& dir) {
        const PokeballType& type = inventory.selectedBall().type;
        Pokeball ball;
        ball.typeId = type.id;
        ball.captureMultiplier = type.captureMultiplier;
        ball.shinyMultiplier = type.shinyMultiplier;
        ball.body.position = handPosition();
        ball.body.velocity = launchVelocity(ball.body.position, aim, dir);

        if (balls.size() >= MAX_BALLS) {
            returnBall(balls.front());
            balls.erase(balls.begin());
        }
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
                ball.roll(h);
                ball.age += h;
                for (CaptureTarget* target : wild.loaded()) {
                    if (!target->hitBy(ball.body.position, Pokeball::RADIUS + Pokeball::CONTACT_MARGIN)) continue;
                    target->beginCapture(ball, CaptureRules::roll(chancePercent(*target, ball.multiplierFor(target->shiny()))), camera.yaw(), world);
                    ball.spent = true;
                    ball.age = Pokeball::LIFETIME; // la bola pasa a ser la de la animación de captura
                    break;
                }
            }
        }
        for (const Pokeball& ball : balls) if (ball.expired()) returnBall(ball);
        balls.erase(std::remove_if(balls.begin(), balls.end(), [](const Pokeball& b) { return b.expired(); }), balls.end());
    }

    // Una bola que no tocó a ningún pokémon vuelve al inventario al desaparecer (la que alcanzó a uno se gasta).
    void returnBall(const Pokeball& ball) {
        if (ball.spent) return;
        const Item* item = m_data->itemOf(ItemCategory::POKEBALL, ball.typeId);
        if (!item || !inventory.add(item->id, 1)) return;
        if (noticeTime <= 0.0f) showNotice(Notice::REWARD, "1x " + m_data->itemName(*item));
    }

    struct Interaction {
        enum Kind { NONE, CHEST, NODE, GROUND, MACHINE, WEAKENED } kind = NONE;
        int index = -1;
    };

    // Hay algún pokémon al alcance al que fijar la cámara (o ya hay uno fijado).
    bool anyLockable() const {
        return lockedTarget || !lockCandidates(nullptr).empty();
    }

    static constexpr const char* CHEST_VERB = "Abrir";
    static constexpr const char* CAPTURE_VERB = "Capturar";
    static constexpr const char* PICK_VERB = "Recoger";
    static constexpr const char* USE_VERB = "Usar";
    inline static const std::string GROUND_NAME = "objeto brillante";
    inline static const std::string MACHINE_NAME = "máquina de investigación";
    static constexpr float CAVE_TEMPERATURE = 12.0f;   // °C dentro de la cueva, la mina y la isla del cielo
    static constexpr float MINE_TEMPERATURE = 14.0f;
    static constexpr float SKY_TEMPERATURE = 8.0f;

    const GameData* m_data;
    float m_clock = 0.0f;       // segundos de partida (marea)
    float m_cascadeTime = 0.0f; // segundos que lleva el jugador en una cascada con un pokémon que la remonta
    static constexpr float CASCADE_RANGE = 5.0f;
    static constexpr float CASCADE_TIME = 3.0f;
    float m_throwDelay = -1.0f; // >= 0: lanzando (la bola sale al llegar a 0)
    int m_fishSkill = -1;       // habilidad de pesca (la de la animación FISH)
    bool m_castHint = false;    // se puede lanzar la caña aquí
    static constexpr float SEEN_RANGE = 30.0f;         // a esta distancia (y a la vista) un pokémon cuenta como visto en la Pokédex
    static constexpr float COLLECT_WINDOW = 8.0f;      // segundos seguidos que recolecta cada vez
    static constexpr float COLLECT_WAIT_MIN = 30.0f;   // segundos que pasa siguiendo al jugador entre una recolección y otra
    static constexpr float COLLECT_WAIT_MAX = 60.0f;
    static constexpr float HOLD_REPEAT = 0.45f;        // segundos entre golpes al mantener pulsado interactuar
    float m_collectClock = 12.0f;
    bool m_collecting = false;
    float m_holdClock = 0.0f;
    int m_shownUid = 0;        // pokémon (uid y especie) que está fuera o saliendo
    int m_shownSpecies = -1;
    int m_missingSkill = -1;   // habilidad que falta para el recurso cercano (-1 = ninguna)
    DirectX::XMFLOAT3 m_missingPos = {};
    int m_missingLevel = 0;    // nivel que exige ese recurso
    Interaction m_interaction; // lo que el jugador puede usar ahora mismo (cofre o recurso cercano)
    DirectX::XMFLOAT3 m_home = {};         // dónde reaparece el jugador al quedarse sin vida
    bool m_concealed = false;              // el jugador está oculto entre matorrales: los pokémon no lo han detectado (sigilo)
    const CaptureTarget* m_weak = nullptr; // salvaje debilitado por el acompañante que se puede capturar ahora (Interaction::WEAKENED)
    bool m_aiming = false; // modo lanzamiento (clic derecho) o L2 mantenido
    float m_lockHidden = 0.0f; // segundos que lleva tapado el pokémon fijado
    CaptureTarget* m_aimTarget = nullptr;  // pokémon al que apunta la cruceta dentro del alcance
};
