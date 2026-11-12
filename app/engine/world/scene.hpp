#pragma once
#include "../../utils/core/crashScope.hpp"
#include <algorithm>
#include <map>
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
#include "../../utils/core/loggerUtil.hpp"
#include <cstdio>
#include "camera.hpp"
#include "style/styleRegistry.hpp"
#include "entities/companion.hpp"
#include "spawn/chestField.hpp"
#include "rules/captureRules.hpp"
#include "rules/companionRules.hpp"
#include "rules/damageRules.hpp"
#include "rules/deathRules.hpp"
#include "entities/captureTarget.hpp"
#include "rules/rewardRules.hpp"
#include "rules/shopRules.hpp"
#include "rules/experienceRules.hpp"
#include "rules/ivRules.hpp"
#include "state/playerProgress.hpp"
#include "rules/pokemonRules.hpp"
#include "state/researchMachine.hpp"
#include "state/professor.hpp"
#include "spawn/groundSpawn.hpp"
#include "spawn/groundField.hpp"
#include "dayCycle.hpp"
#include "habitat/habitatMap.hpp"
#include "habitat/weatherSystem.hpp"
#include "spawn/wildSpawn.hpp"
#include "spawn/wildField.hpp"
#include "entities/fruit.hpp"
#include "geometry.hpp"
#include "../audio/radio.hpp"
#include "../audio/sounds.hpp"
#include "footprints.hpp"
#include "state/exploration.hpp"
#include "state/fishing.hpp"
#include "state/inventory.hpp"
#include "entities/player.hpp"
#include "entities/playerAction.hpp"
#include "entities/pokeball.hpp"
#include "state/pokemonStorage.hpp"
#include "state/battle.hpp"
#include "entities/trainer.hpp"
#include <memory>
#include "spawn/resourceSpawn.hpp"
#include "spawn/cropField.hpp"
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
    std::vector<ResourceNode> nodes; // materiales de recolección, colocados al generar el mundo (al final, los nodos vacíos de las parcelas)
    std::vector<Crops::Plot> plots;  // parcelas de cultivo junto al inicio
    Exploration exploration;  // lo que el jugador ha explorado (el mapa grande lo muestra)
    Fishing fishing;          // pesca: corcho, picada y pokémon enganchado
    SpawnField<GroundItem> groundItems;
    std::vector<Fruit> fruits; // cocos y plátanos caídos de los árboles (pocos a la vez)
    Footprints footprints;     // huellas en la arena de todo lo que se mueve
    int radioStation = 0;      // emisora sintonizada (0 = radio apagada)
    // Poción que el jugador está usando sobre su acompañante (la dibuja Renderer3D::addPotionFx: en su mano, el spray y el brillo del pokémon).
    struct PotionFx { std::string model; DirectX::XMFLOAT3 tint = { 1.0f, 1.0f, 1.0f }; float time = -1.0f; bool sprayed = false; bool give = false; }; // give: revivir (se lo da al pokémon, sin spray)
    static constexpr float POTION_FX_TIME = 2.2f, POTION_SPRAY_FROM = 0.55f, POTION_SPRAY_TO = 1.75f, POTION_DISTANCE = 1.7f, GIVE_FROM = 0.4f, GIVE_TO = 1.0f;
    PotionFx potionFx;
    bool radioOpen = false;    // rueda de emisoras abierta (con ella la mira queda bloqueada y el ratón elige)
    float radioFade = 0.0f;    // apertura visible de la rueda (0..1)
    float radioCursorX = 0.0f, radioCursorY = 0.0f; // cursor de la rueda, en píxeles respecto al centro
    int radioHover = -1;       // emisora señalada (-1 ninguna, -2 fuera de la rueda)
    PokemonStorage storage;   // equipo (6) y PC
    PlayerProgress progress;  // nivel, experiencia y medallas del jugador
    PlayerHealth health;      // vida del jugador (caídas, ahogamiento)
    ResearchMachine machine;  // máquina de investigación (fija)
    Professor oak;            // Profesor Oak (fijo junto al inicio)
    bool starterRequested = false; // el jugador ha hablado con Oak sin tener pokémon: la interfaz abre la elección del inicial
    bool seedRequested = false;     // junto a una parcela con varias semillas: la interfaz abre la pantalla de elección
    bool researchRequested = false; // el jugador ha usado la máquina: la interfaz abre su pantalla
    std::vector<Trainer> trainers;  // entrenadores de prueba junto al inicio
    std::shared_ptr<Battle> battle; // combate por turnos en curso (nullptr = ninguno)
    bool battleRequested = false;   // se acaba de empezar un combate por turnos: la interfaz abre su pantalla
    bool shopRequested = false;     // se acaba de hablar con el tendero: la interfaz abre la tienda
    bool learnRequested = false;    // hay un pokémon con un movimiento esperando decisión: la interfaz abre su pantalla
    // Animación del combate por turnos: quién ataca (0 jugador, 1 rival), si el golpe llega y el tiempo desde que empezó el mensaje.
    struct BattleFx { int attacker = -1; int target = -1; bool hit = false; float time = 0.0f; };
    BattleFx battleFx;
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
        plots = Crops::generate(gameData, world, nodes);
        trainers.push_back(Trainer("Entrenador Rojo", Trainer::Style::REAL_TIME, -1.0f, 10.0f, { 0.85f, 0.2f, 0.2f }));
        trainers.push_back(Trainer("Entrenador Azul", Trainer::Style::TURN_BASED, -5.0f, 10.0f, { 0.2f, 0.35f, 0.85f }));
        trainers.back().gym = true; // líder de prueba: al derrotarlo da una medalla de gimnasio
        trainers.push_back(Trainer("Entrenador Verde", Trainer::Style::DOUBLES, 2.0f, 10.0f, { 0.25f, 0.7f, 0.3f }));
        trainers.push_back(Trainer("Tendero", Trainer::Style::SHOP, 5.0f, 10.0f, { 0.2f, 0.62f, 0.38f }));
        m_home = player.body.position;
        for (const Skill& skill : gameData.skills) if (PlayerActions::parse(skill.animation) == PlayerAction::FISH) m_fishSkill = skill.id;
    }

    // Devuelve true si hay que pausar el juego (ESC fuera del modo lanzamiento).
    bool update(float dt, const InputState& rawInput) {
        CRASH_SCOPE();
        const InputState blank;
        const InputState& input = m_respawn > 0.0f ? blank : rawInput; // mientras carga tras desmayarse no se controla nada
        bool pause = false;
        if (input.escape) {
            if (radioOpen) closeRadio();
            else if (aimMode) aimMode = false;
            else pause = true;
        }
        m_clock += dt;
        world.time = m_clock;
        world.tide = TIDE_AMPLITUDE * std::sin(m_clock * 6.2831853f / TIDE_PERIOD);
        const bool hasBalls = inventory.ownedCount() > 0; // sin pokéballs (de ninguna clase) no hay modo captura; si se agotan, se sale de él
        if (input.aimToggle) {
            aimMode = !aimMode && hasBalls;
            if (!hasBalls && !input.aimHold) showNotice(Notice::REWARD, "No tienes Pokéballs.");
        }
        if (!hasBalls) aimMode = false;
        m_aiming = (aimMode || input.aimHold) && hasBalls;
        if (m_aiming != m_wasAiming) {
            if (m_aiming) inventory.selectFirst();
            Logger::logInfo("CAPTURE_MODE", std::string(m_aiming ? "Entra" : "Sale") + " del modo captura | pokéballs con unidades=" + std::to_string(inventory.ownedCount()) + " | elegida=" + (inventory.hasBalls() ? inventory.selectedBall().type.name : std::string("-")));
            m_wasAiming = m_aiming;
        }
        updatePotionFx(dt);
        const bool wheel = updateRadioWheel(dt, input); // con la rueda abierta la mira y el lanzamiento quedan bloqueados
        if (input.ballSwitch != 0 && !wheel) {
            if (m_aiming) inventory.cycle(input.ballSwitch);
            else storage.cycleActive(input.ballSwitch); // fuera del modo captura cambia el pokémon que acompaña
        }
        if (input.modeSwitch) {
            storage.cycleMode();
            showNotice(Notice::REWARD, std::string("Modo: ") + CompanionRules::label(storage.mode));
        }
        if (input.teamSelect > 0) storage.setActive(input.teamSelect - 1); // la misma tecla que el que ya está fuera no cambia nada

        wild.update(dt, player.body.position.x, player.body.position.z, camera.yaw(), player.body.underground, *m_data, habitats, [this](CaptureTarget& target) { return assignSpecies(target); },
                    [this](float x, float y, float z) { return canSee(x, y, z); });

        for (const CaptureTarget* target : wild.loaded()) { // lo que se ve de cerca queda registrado en la Pokédex
            if (target->hittable() && target->speciesIndex() >= 0 && nearPlayer(target->body.position, SEEN_RANGE) && visible(*target)) storage.markSeen(target->speciesId());
        }

        if (input.lockCancel) lockedTarget = nullptr;
        else if (input.lockTap) cycleLock();
        validateLock(dt);

        m_swimLatch = (player.swimming() || player.diving()) ? 1.5f : (std::max)(0.0f, m_swimLatch - dt); // sigue unos instantes tras salir del agua (la orilla alterna)
        camera.setSwimming(m_swimLatch > 0.0f);
        camera.update(dt, m_aiming);
        if (lockedTarget) camera.trackToward(player.body.position, lockedTarget->center(), dt);
        else if (!wheel) camera.rotate(input.lookX, input.lookY);
        logSwimCamera(dt, input);

        // Los pokémon son sólidos para quien los pisa: el jugador los rodea igual que a las paredes.
        world.creatures.clear();
        for (const CaptureTarget* target : wild.loaded()) {
            if (target->hittable()) world.creatures.push_back(target->solid());
            else if (const CaptureSequence* seq = target->sequence()) { // la pokéball que captura también es sólida
                const CaptureSequence::BallPose pose = seq->ballPose();
                if (pose.scale > 0.2f) world.creatures.push_back({ pose.position, { BALL_SOLID, BALL_SOLID, BALL_SOLID }, false });
            }
        }
        world.props.clear();
        for (const Chest& chest : chests.entities) if (chest.closed()) world.props.push_back(chest.solid());
        for (const ResourceNode& node : nodes) if (!node.gone() && !node.vein() && nearPlayer(node.body.position, PROP_RANGE)) world.props.push_back(node.solid());
        world.props.push_back(machine.solid());
        world.props.push_back(oak.solid());
        for (const Trainer& trainer : trainers) world.props.push_back(trainer.solid());

        player.aiming = m_aiming || camera.aimBlend() > 0.05f; // el personaje deja de apuntar cuando la cámara ya casi ha vuelto
        InputState moveInput = input;
        if (fishing.hooked()) moveInput.crouch = moveInput.crouchHeld = false; // los botones de decidir (Liberar) no agachan
        {   // el compañero con branquias da aire al jugador bajo el agua
            const OwnedPokemon* lead = storage.active();
            const PokemonSpecies* gills = lead ? m_data->speciesById(lead->speciesId) : nullptr;
            player.breathesUnderwater = gills && companion.active() && !companion.changing() && m_data->breathesUnderwater(*gills);
        }
        player.onCloud = habitats.cloudFloor(player.body.position.x, player.body.position.z, player.body.feet(), true);
        player.update(dt, moveInput, camera.yaw(), camera.lookAngle(), world);
        updateHealth(dt);

        weather.update(dt);
        throwCooldown = (std::max)(0.0f, throwCooldown - dt);
        noticeTime = (std::max)(0.0f, noticeTime - dt);
        if (m_aiming && input.throwBall && !wheel && !aimingAtTrainerPokemon()) m_throwQueued = THROW_BUFFER; // el clic se recuerda unos instantes: lanzar no depende de si caminamos, corremos o nadamos
        m_throwQueued = (std::max)(0.0f, m_throwQueued - dt);
        if (!m_aiming) m_throwQueued = 0.0f;
        if (m_throwQueued > 0.0f && throwCooldown <= 0.0f && m_throwDelay < 0.0f) { m_throwQueued = 0.0f; startThrow(); }
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
        m_flowerTimer += dt;
        if (m_flowerTimer >= FLOWER_CHECK_SECONDS) {
            m_flowerTimer = 0.0f;
            if (!player.body.underground) ResourceSpawn::regrowFlower(nodes, *m_data, habitats, world, player.body.position.x, player.body.position.z, m_flowerWeights);
        }
        for (ResourceNode& node : nodes) {
            node.update(dt);
            if (!m_concealed && concealsPlayer(node)) m_concealed = true;
        }
        exploration.reveal(player.body.position.x, player.body.position.z);
        groundItems.update(dt, GroundSpawn::DELAY,
            [&](GroundItem& item) { world.step(item.body, dt); item.update(dt); },
            [&](int spot) { return GroundSpawn::make(*m_data, spot); });
        GroundField::update(dt, m_groundTimer, player.body.position.x, player.body.position.z, player.body.underground, *m_data, habitats, world, groundItems.entities);
        updateFruits(dt);
        updateCompanion(dt);
        updateTrainerDuel(dt);
        updateLearning();
        updateHeldItems(dt);
        updateForage(dt);
        updateFootprints(dt);
        updateStrikes(dt);
        updateWeakTimer(dt);
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

    // Crea el pokémon inicial: nivel inicial, analizado, con su pokéball exclusiva y sus 3 mejores estadísticas (las suyas, no
    // las de su evolución) con el EV máximo al máximo; al evolucionar el S+ se adapta a la nueva forma.
    void chooseStarter(int speciesId) {
        const PokemonSpecies* species = m_data->speciesById(speciesId);
        const PokeballType* ball = m_data->starterBall();
        if (!species || !needsStarter()) return; // la interfaz decide qué se ofrece (iniciales o reserva Pichu/Eevee)

        OwnedPokemon owned;
        owned.speciesId = species->id;
        owned.level = PokemonRules::STARTER_LEVEL;
        owned.ballId = ball ? ball->id : -1;
        owned.analyzed = true;
        owned.shiny = RandomUtil::roll(PokemonRules::STARTER_SHINY_CHANCE);
        owned.ivCaps = IvRules::starterCaps();
        owned.nature = m_data->randomNatureId();
        bool sentToPc = false;
        storage.add(owned, sentToPc);
    }

    DirectX::XMMATRIX getViewMatrix() const {
        if (battle) return battleView();
        const DirectX::XMFLOAT3 at = camera.focus(player.body.position), full = camera.eye(player.body.position);
        const float feet = player.body.feet();
        const bool inWater = m_swimLatch > 0.0f;
        float pull = (std::max)(world.clearFraction(at, full, 0.5f), Camera::MIN_PULL); // una pared (galerías) acerca la cámara
        if (inWater) pull = (std::min)(pull, swimPull(at, full, feet)); // en el agua el fondo acerca la cámara, nunca la obliga a mirar en otra dirección
        const DirectX::XMFLOAT3 eye = { at.x + (full.x - at.x) * pull, at.y + (full.y - at.y) * pull, at.z + (full.z - at.z) * pull };
        const float ceiling = world.under(eye.x, eye.z, feet) ? world.ceiling(eye.x, eye.z) - 0.3f : Physics::World::ABOVE;
        const float minEye = inWater ? -1.0e9f : world.surfaceHeight(eye.x, eye.z, feet) + 0.35f;
        m_viewPull = pull;
        m_viewEyeY = eye.y;
        return camera.viewMatrix(player.body.position, minEye, ceiling, pull); // la cámara no se hunde en el terreno (bajo el agua sí se sumerge)
    }

    // Fracción (0..1) del trayecto del punto de mira a la cámara que queda libre de fondo marino.
    float swimPull(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& full, float feet) const {
        constexpr int STEPS = 24;
        float last = 1.0f;
        for (int i = 1; i <= STEPS; ++i) {
            const float t = static_cast<float>(i) / STEPS;
            const float x = at.x + (full.x - at.x) * t, y = at.y + (full.y - at.y) * t, z = at.z + (full.z - at.z) * t;
            if (y < world.groundHeight(x, z, feet) + 0.25f) { last = static_cast<float>(i - 1) / STEPS; break; }
        }
        return (std::max)(last, Camera::MIN_PULL);
    }

    // Traza de la cámara en el agua: una línea cada medio segundo (y siempre que se mueva el ratón) en el log.
    void logSwimCamera(float dt, const InputState& input) {
        if (m_swimLatch <= 0.0f) { m_swimLogTime = 0.0f; return; }
        m_swimLogTime += dt;
        const bool looking = std::fabs(input.lookX) + std::fabs(input.lookY) > 0.0f;
        if (m_swimLogTime < 0.5f && !(looking && m_swimLogTime > 0.1f)) return;
        m_swimLogTime = 0.0f;
        char text[512];
        std::snprintf(text, sizeof(text),
            "swim=%d dive=%d latch=%.2f lock=%d aim=%d look=(%.1f,%.1f) yaw=%.3f pitch=%.3f minPitch=%.3f pull=%.2f eyeY=%.2f feetY=%.2f move=(%.1f,%.1f) vy=%.2f",
            player.swimming(), player.diving(), m_swimLatch, lockedTarget != nullptr, m_aiming, input.lookX, input.lookY,
            camera.yaw(), camera.pitch(), camera.lowestPitch(), m_viewPull, m_viewEyeY, player.body.feet(), input.moveX, input.moveY, player.body.velocity.y);
        Logger::logInfo("CAMERA_WATER", text);
    }

    GameStatus status() const {
        const Interaction ia = validInteraction();
        GameStatus s;
        s.aimBlend = camera.aimBlend();
        s.loading = loadingFade();
        s.deathTitle = &m_deathTitle;
        s.deathDetail = &m_deathDetail;
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
        s.interactTime = ia.kind == Interaction::WEAKENED ? (std::max)(1.0f - m_weakAge / CompanionRules::WEAK_PROMPT_TIME, 0.0f) : -1.0f;
        s.interactOnScreen = ia.kind == Interaction::WEAKENED; // el aviso de capturar al debilitado va fijo en la pantalla del jugador
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
                s.team.push_back({ species, owned.level, owned.shiny, owned.ballId, static_cast<int>(i) == storage.activeIndex(), storage.fainted(owned) });
            }
        }
        if (!noticeText.empty()) s.noticeText = &noticeText;
        if (ia.kind == Interaction::WEAKENED && m_weak) {
            const PokemonSpecies* species = m_data->speciesById(m_weak->speciesId());
            s.interactVerb = CAPTURE_VERB;
            s.interactTarget = species ? &species->name : &GROUND_NAME;
        } else if (ia.kind == Interaction::CHEST) {
            s.interactVerb = CHEST_VERB;
            s.interactTarget = &m_data->chests[chests.entities[ia.index].typeIndex()].name;
        } else if (ia.kind == Interaction::PLOT) {
            s.interactVerb = PLANT_VERB;
            const std::vector<int> seedCrops = availableSeeds();
            const Material* seeds = seedCrops.size() == 1 ? seedMaterial(seedCrops.front()) : nullptr;
            s.interactTarget = seeds ? &seeds->name : &PLOT_NAME;
        } else if (ia.kind == Interaction::NODE) {
            const ResourceNode& node = nodes[ia.index];
            const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
            const Skill* skill = node.growing() ? m_data->skill(type.skillId) : nullptr;
            s.interactVerb = skill ? skill->name.c_str() : type.action.c_str();
            s.interactTarget = &type.name;
            const Material* material = m_data->material(type.materialId);
            if (!skill && material && type.action == PICK_VERB) s.interactTarget = &material->name; // "Recoger Baya"; los árboles siguen en "Talar Roble"
        } else if (ia.kind == Interaction::GROUND) {
            s.interactVerb = PICK_VERB;
            s.interactTarget = &GROUND_NAME;
            const GroundItem& item = groundItems.entities[ia.index];
            if (item.reward() >= 0) {
                if (const Item* found = m_data->rewardItem(m_data->rewards[item.reward()])) s.interactTarget = &m_data->itemName(*found);
            }
        } else if (ia.kind == Interaction::FRUIT) {
            s.interactVerb = PICK_VERB;
            s.interactTarget = &fruitName(fruits[ia.index]);
        } else if (ia.kind == Interaction::PROFESSOR) {
            s.interactVerb = TALK_VERB;
            s.interactTarget = &PROFESSOR_NAME;
        } else if (ia.kind == Interaction::TRAINER) {
            s.interactVerb = trainers[ia.index].style == Trainer::Style::SHOP ? SHOP_VERB : FIGHT_VERB;
            s.interactTarget = &trainers[ia.index].name;
        } else if (ia.kind == Interaction::MACHINE) {
            s.interactVerb = USE_VERB;
            s.interactTarget = &MACHINE_NAME;
        }
        s.radioStation = radioStation;
        s.radioFade = radioFade;
        s.radioCursorX = radioCursorX; s.radioCursorY = radioCursorY;
        s.radioHover = radioHover;
        if (m_aimTarget) {
            const CaptureTarget& t = *m_aimTarget;
            s.hasAimTarget = true;
            s.aimOwned = aimingAtTrainerPokemon();
            s.behind = isBehind(t);
            s.hidden = m_concealed;
            s.eating = t.eating();
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
        s.weatherId = current ? current->id : 1;
    }

    // Temperatura, altura, aire, región y ayudas inferiores.
    void fillWorldState(GameStatus& s) const {
        const DirectX::XMFLOAT3& p = player.body.position;
        const HabitatMap::Region* region = habitats.regionAt(p.x, p.z);
        s.temperature = temperatureAt(p.x, p.z);
        s.burning = health.burning();
        s.spring = m_pool < 0 ? 0 : m_poolTime < HealthRules::SPRING_SAFE_TIME ? 1 : 2;
        s.springLeft = (std::max)(0.0f, HealthRules::SPRING_SAFE_TIME - m_poolTime);
        s.altitude = (std::max)(0.0f, p.y);
        s.health = health.fraction();
        s.hurt = health.hurtFlash();
        s.heal = health.healFlash();
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

    // °C donde está el jugador: las regiones tienen su clima; en tierra baja con la altura (menos en zonas volcánicas) y cada hábitat suma el
    // suyo (la lava, las aguas termales y los volcanes calientan; el hielo enfría).
    float temperatureAt(float x, float z) const {
        const bool deep = player.body.underground;
        float base;
        if (deep) {
            const Underground::System* system = habitats.systemAt(x, z);
            base = system && system->kind == Underground::Kind::MINE ? MINE_TEMPERATURE : CAVE_TEMPERATURE;
        } else if (const HabitatMap::Region* region = habitats.regionAt(x, z)) {
            switch (region->kind) {
                default: return SKY_TEMPERATURE;
            }
        } else {
            base = 0.0f;
        }
        const float altitude = (std::max)(world.groundHeight(x, z), 0.0f);
        std::vector<float> weights;
        habitats.weights(x, z, weights, deep);
        float sum = 0.0f, temperature = 0.0f;
        for (size_t h = 0; h < weights.size() && h < m_data->habitats.size(); ++h) {
            const Habitat& habitat = m_data->habitats[h];
            const float local = deep ? base : HabitatMap::temperature(altitude, habitat.altitudeCooling);
            temperature += weights[h] * (local + habitat.warmth);
            sum += weights[h];
        }
        temperature = sum > 0.0f ? temperature / sum : (deep ? base : HabitatMap::temperature(altitude));
        if (!deep) {
            temperature += HealthRules::LAVA_HEAT * world.lavaHeat(x, z);
            if (world.springAt(x, z, player.body.feet()) >= 0) temperature += HealthRules::SPRING_HEAT;
        }
        return (std::max)(temperature, HealthRules::MIN_TEMPERATURE);
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
                    add(PromptAction::INTERACT, inventory.basicIndex() >= 0 ? "Capturar con " + inventory.ball(inventory.basicIndex()).type.name : "Capturar (sin pokéballs)", inventory.basicIndex() >= 0);
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
            if (species && Geometry::within(head, player.body.position, TAG_RANGE)) {
                s.nameTags.push_back({ { head.x, head.y + TAG_LIFT, head.z }, (shiny ? "* " : "") + species->name + "  Nv. " + std::to_string(level) +
                    (hp < 1.0f ? "  PS " + std::to_string(static_cast<int>(std::ceil(hp * 100.0f))) + "%" : std::string()), shiny });
            }
        };
        for (const CaptureTarget* target : wild.loaded()) {
            if (target->hittable()) add(target->speciesId(), target->level(), target->shiny(), { target->body.position.x, target->body.position.y + CaptureTarget::SIZE, target->body.position.z }, target->hpFraction());
        }
        if (companion.active() && !companion.changing() && storage.active()) {
            const OwnedPokemon& lead = *storage.active();
            add(companion.speciesId(), lead.level, companion.shiny(), { companion.body.position.x, companion.body.position.y + Companion::SIZE, companion.body.position.z }, companionHpFraction());
        }
    }

    bool nearPlayer(const DirectX::XMFLOAT3& at, float range) const {
        return Geometry::within(at, player.body.position, range);
    }

    // Punto del mundo sobre lo que se interactúa, donde se dibuja la ayuda de cómo hacerlo.
    DirectX::XMFLOAT3 interactionAnchor() const {
        const Interaction ia = validInteraction();
        const auto above = [](const Physics::Body& body, float height) { return DirectX::XMFLOAT3{ body.position.x, body.position.y + height, body.position.z }; };
        switch (ia.kind) {
            case Interaction::CHEST:   return above(chests.entities[ia.index].body, Chest::HEIGHT + 0.5f);
            case Interaction::NODE:    return nodeAnchor(nodes[ia.index]);
            case Interaction::PLOT:    return { plots[ia.index].position.x, plots[ia.index].position.y + 0.9f, plots[ia.index].position.z };
            case Interaction::GROUND:  return above(groundItems.entities[ia.index].body, 0.9f);
            case Interaction::FRUIT:   return above(fruits[ia.index].body, 0.7f);
            case Interaction::MACHINE: return above(machine.body, ResearchMachine::HEIGHT + 0.5f);
            case Interaction::PROFESSOR: return above(oak.body, Professor::HEIGHT + 0.5f);
            case Interaction::TRAINER: return above(trainers[ia.index].body, Trainer::HEIGHT + 0.5f);
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
        const float impact = player.takeImpact();
        const DirectX::XMFLOAT3& at = player.body.position;
        const bool cloud = habitats.cloudFloor(at.x, at.z, player.body.feet()), bounce = habitats.cloudFloor(at.x, at.z, player.body.feet(), true); // rebota solo en el suelo de nube; en toda la isla no hay daño
        if (bounce && impact >= HealthRules::CLOUD_BOUNCE_MIN) player.body.velocity.y = impact * HealthRules::CLOUD_BOUNCE; // la nube rebota según lo alto que se cayó y cada rebote es menor
        if (!cloud) health.hurt(HealthRules::fallDamage(impact), DeathRules::Cause::FALL); // el suelo de nube no hace daño
        if (player.drowning()) health.hurt(HealthRules::DROWN_PER_SECOND * dt, DeathRules::Cause::DROWN);
        updateBurn(dt);
        updateSpring(dt);
        if (m_respawn <= 0.0f && (health.dead() || storage.teamWiped())) { // desmayado: pantalla de carga y, ya a oscuras, vuelta al inicio
            m_respawn = RESPAWN_TIME;
            m_respawnMoved = false;
            m_deathTitle = DeathRules::title(health.dead() ? health.lastCause() : DeathRules::Cause::TEAM);
            m_deathDetail = DeathRules::detail(RESPAWN_PLACE);
            showNotice(Notice::FAINTED);
        }
        if (m_respawn <= 0.0f) return;
        m_respawn = (std::max)(0.0f, m_respawn - dt);
        if (!m_respawnMoved && m_respawn <= RESPAWN_TIME * 0.5f) {
            m_respawnMoved = true;
            player.body.position = m_home;
            player.body.velocity = { 0.0f, 0.0f, 0.0f };
            health.restore();
            storage.healTeam(); // al volver al inicio, el equipo se cura
        }
    }

    // 0 = sin pantalla de carga; 1 = pantalla completa (se funde a negro, espera y vuelve).
    float loadingFade() const {
        if (m_respawn <= 0.0f) return 0.0f;
        return (std::min)((RESPAWN_TIME - m_respawn) / 0.7f, 1.0f) * (std::min)(m_respawn / 0.9f, 1.0f);
    }

    // Lava: daño y quemadura. La quemadura se apaga con agua o deslizándose por el suelo.
    void updateBurn(float dt) {
        const DirectX::XMFLOAT3& at = player.body.position;
        if (world.inLava(at.x, at.z, player.body.feet())) {
            health.hurt(HealthRules::LAVA_PER_SECOND * dt, DeathRules::Cause::LAVA);
            health.ignite();
        } else if (health.burning() && (player.sliding() || world.waterDepth(at.x, at.z, player.body.feet()) > 0.2f)) {
            health.extinguish();
        }
    }

    // Aguas termales: cada charca tiene su temporizador, que empieza de 0 al entrar en ella. Mientras no vence cura poco a poco; después daña.
    void updateSpring(float dt) {
        const DirectX::XMFLOAT3& at = player.body.position;
        const int pool = world.springAt(at.x, at.z, player.body.feet());
        if (pool != m_pool) { m_pool = pool; m_poolTime = 0.0f; }
        if (pool < 0) return;
        m_poolTime += dt;
        if (m_poolTime < HealthRules::SPRING_SAFE_TIME) {
            health.heal(HealthRules::SPRING_HEAL_PER_SECOND * dt);
            healCompanion(HealthRules::SPRING_HEAL_PER_SECOND * 0.01f * dt); // el compañero también se cura en el agua caliente
        }
        else health.hurt(HealthRules::SPRING_HURT_PER_SECOND * dt, DeathRules::Cause::SPRING);
    }

    bool concealsPlayer(const ResourceNode& node) const {
        if (ResourceStyle::look(node.typeId()).shape != ResourceStyle::Shape::BUSH) return false;
        return Geometry::within(node.body.position, player.body.position, CONCEAL_RADIUS) && std::fabs(node.body.position.y - player.body.position.y) < 1.5f;
    }

    bool isBehind(const CaptureTarget& t) const {
        return CaptureRules::isBehind(t.body.position, t.yaw(), player.body.position);
    }

    float chancePercent(const CaptureTarget& t, float ballMultiplier) const {
        const PokemonSpecies* species = m_data->speciesById(t.speciesId());
        const float base = species ? CaptureRules::basePercent(species->catchRate, t.level(), progress.level()) * CaptureRules::hpFactor(t.hpFraction()) : 0.0f;
        return CaptureRules::percent(base, isBehind(t), m_concealed, ballMultiplier, t.eating());
    }

    // Recoge lo usado desde el menú sobre un pokémon: una poción sobre el acompañante la usa el jugador (se vuelve hacia él, el pokémon se acerca, le rocía
    // y la guarda); sobre otro del equipo solo suena. Una evolución por objeto suena igual.
    void updatePotionFx(float dt) {
        PokemonStorage::MedicineUse use;
        if (storage.takeMedicine(use)) {
            const bool onActive = use.inTeam && use.index == storage.activeIndex();
            const TrainingItem* type = m_data->training(use.trainingId);
            if (use.onCompanion && storage.active()) m_shownUid = storage.active()->uid; // sobre el acompañante nunca se recoge ni vuelve a salir
            if (use.evolved) {
                if (onActive && storage.active()) m_itemEvolveUid = storage.active()->uid; // el efecto de evolución lo hace el acompañante en cuanto está fuera
                else Sound::emit(Sound::Sfx::EVOLVE, player.body.position);
            } else if (onActive && type && storage.active()) {
                m_pendingPotion = { type, storage.active()->uid, 6.0f }; // la usa en cuanto el pokémon está fuera (si salía otro, primero vuelve a su bola)
            } else Sound::emitGlobal(Sound::Sfx::HEAL);
        }
        if (m_pendingPotion.type) {
            const OwnedPokemon* lead = storage.active();
            m_pendingPotion.wait -= dt;
            if (!lead || lead->uid != m_pendingPotion.uid || m_pendingPotion.wait <= 0.0f) m_pendingPotion.type = nullptr;
            else if (lead->uid == m_shownUid && companion.active() && companion.speciesId() == lead->speciesId && !companion.changing() && !companion.tripActive() && potionFx.time < 0.0f) {
                const TrainingItem& type = *m_pendingPotion.type;
                potionFx = { type.asset, type.rgb >= 0 ? StyleRegistry::unpack(type.rgb) : DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f }, 0.0f, false, type.effect == TrainingItem::Effect::REVIVE };
                player.act(PlayerAction::POTION);
                m_pendingPotion.type = nullptr;
            }
        }
        if (potionFx.time < 0.0f) return;
        potionFx.time += dt;
        const DirectX::XMFLOAT3& pet = companion.body.position;
        player.actionYaw = std::atan2(pet.x - player.body.position.x, pet.z - player.body.position.z);
        if (!potionFx.sprayed && potionFx.time >= (potionFx.give ? GIVE_TO : POTION_SPRAY_FROM)) { potionFx.sprayed = true; Sound::emit(Sound::Sfx::HEAL, companion.center()); }
        if (potionFx.time > POTION_FX_TIME) potionFx.time = -1.0f;
    }

    // Rueda de emisoras. T la abre / cierra; con ella abierta el ratón (o el stick) mueve un cursor en lugar de la cámara, el clic sobre una emisora
    // la sintoniza y cierra la rueda con calma, y el clic fuera del círculo la cierra sin cambiar nada. Devuelve true mientras controla la entrada.
    bool updateRadioWheel(float dt, const InputState& input) {
        if (input.radio) { if (radioOpen) closeRadio(); else openRadio(); }
        if (m_radioHold > 0.0f && (m_radioHold -= dt) <= 0.0f) radioOpen = false; // tras elegir, la rueda aguanta un instante y se desvanece
        radioFade = approach(radioFade, radioOpen ? 1.0f : 0.0f, dt / (radioOpen ? RADIO_FADE_IN : RADIO_FADE_OUT));
        if (!radioOpen || m_radioHold > 0.0f) return radioOpen;
        radioCursorX += input.lookX;
        radioCursorY += input.lookY;
        const float length = std::sqrt(radioCursorX * radioCursorX + radioCursorY * radioCursorY);
        if (length > Radio::Wheel::REACH) { radioCursorX *= Radio::Wheel::REACH / length; radioCursorY *= Radio::Wheel::REACH / length; }
        radioHover = Radio::Wheel::hit(radioCursorX, radioCursorY);
        if (input.throwBall) {
            if (radioHover >= 0) tuneRadio(radioHover);
            else if (radioHover == -2) closeRadio();
        }
        return true;
    }

    void openRadio() { radioOpen = true; m_radioHold = 0.0f; radioCursorX = radioCursorY = 0.0f; radioHover = -1; }
    void closeRadio() { radioOpen = false; m_radioHold = 0.0f; }

    // Sintoniza una emisora (0 = apagada) con una interferencia, y cierra la rueda sin brusquedad.
    void tuneRadio(int station) {
        if (station != radioStation) Sound::emitGlobal(Sound::Sfx::RADIO_TUNE);
        radioStation = station;
        m_radioHold = RADIO_PICK_HOLD;
    }

    static float approach(float value, float target, float step) { return value < target ? (std::min)(value + step, target) : (std::max)(value - step, target); }

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
            owned.ivCaps = IvRules::rollCaps(CaptureRules::minimumRank(target.throwKind()));
            owned.nature = m_data->randomNatureId();
            bool sentToPc = false;
            const bool stored = storage.add(owned, sentToPc);
            text = (owned.shiny ? "* " : "") + species->name + "  Nv. " + std::to_string(owned.level) +
                   (!stored ? " (sin espacio)" : sentToPc ? " (enviado al PC)" : "");
        }
        if (const int levels = progress.addXp(static_cast<int>(std::lround(static_cast<float>(Xp::CAPTURE) * rarityXp))); levels > 0) text += "   ¡Nivel " + std::to_string(progress.level()) + "!";
        if (event == CaptureTarget::Event::CAPTURED && storage.active()) { // el compañero también gana (menos que si lo derrotara: misma fórmula, reducida)
            const int xp = storage.active()->level >= progress.levelCap() ? 0 : ExperienceRules::pokemonXp(target.level(), storage.active()->level, 1, rarityXp, ExperienceRules::Source::CAPTURE);
            storage.addXp(storage.activeIndex(), xp, progress.levelCap());
            if (xp > 0) text += "   +" + std::to_string(xp) + " PX";
        }
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
    // Siempre con la pokéball básica, sea cual sea la elegida en la rueda.
    void captureDirect(const CaptureTarget& wildTarget) {
        const int basic = inventory.basicIndex();
        if (basic < 0) {
            return;
        }
        const DirectX::XMFLOAT3 target = wildTarget.center();
        const DirectX::XMFLOAT3 hand = handPosition();
        DirectX::XMFLOAT3 dir = { target.x - hand.x, target.y - hand.y, target.z - hand.z };
        const float length = (std::max)(std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z), 1.0e-3f);
        dir = { dir.x / length, dir.y / length, dir.z / length };
        player.act(PlayerAction::THROW);
        launch(target, dir, basic);
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

    // Un pokémon que evoluciona al remontar una cascada (Magikarp) que pasa cerca de una va hacia ella para intentar subirla: cada intento
    // tiene una probabilidad de éxito (si lo logra, evoluciona) y, si falla, descansa un rato antes de volver a intentarlo.
    // Devuelve el punto de la cascada hacia el que va (nullptr si no hay intento en marcha).
    const DirectX::XMFLOAT3* updateWaterfall(float dt, const PokemonSpecies* species) {
        m_cascadeRest = (std::max)(0.0f, m_cascadeRest - dt);
        const bool evolveReady = companion.takeEvolveReady();
        if (evolveReady && storage.active() && storage.active()->speciesId != companion.speciesId()) { // evolución por objeto: cambia de especie en pleno destello
            const PokemonSpecies* next = m_data->speciesById(storage.active()->speciesId);
            m_shownSpecies = storage.active()->speciesId;
            m_itemEvolveUid = 0; // evolución por objeto terminada: de ahí en adelante un cambio de especie sí cambiaría de pokémon
            if (next) companion.morph(next->id, m_data->levitates(*next));
        } else if (evolveReady && storage.active()) { // en lo alto de la cascada, en pleno destello
            const std::string before = storage.active()->speciesId >= 0 ? m_data->speciesById(storage.active()->speciesId)->name : std::string();
            if (const PokemonSpecies* next = storage.evolveBy(storage.activeIndex(), Evolution::Method::WATERFALL)) {
                m_shownSpecies = next->id; // misma criatura: no hay cambio de pokéball
                companion.morph(next->id, m_data->levitates(*next));
                showNotice(Notice::REWARD, "¡" + before + " ha remontado la cascada y ha evolucionado a " + next->name + "!");
            }
        }
        if (companion.tripActive()) return nullptr;
        bool climbs = false;
        if (species) for (const Evolution& e : species->evolutions) climbs = climbs || e.method == Evolution::Method::WATERFALL;
        {   // el jugador remonta una cascada nadando con él al lado: tras unos segundos evoluciona
            float px = 0.0f, pz = 0.0f;
            const DirectX::XMFLOAT3& p = player.body.position;
            const float cx = companion.body.position.x - p.x, cz = companion.body.position.z - p.z;
            const bool together = cx * cx + cz * cz <= CASCADE_TOGETHER * CASCADE_TOGETHER;
            if (climbs && !companionDown() && !companion.changing() && together && (player.swimming() || player.diving()) && habitats.nearestCascade(p.x, p.z, CASCADE_PLAYER_RANGE, px, pz)) {
                m_cascadeTime += dt;
                if (m_cascadeTime >= CASCADE_TIME) { m_cascadeTime = 0.0f; companion.beginEvolve(); return nullptr; }
            } else {
                m_cascadeTime = (std::max)(0.0f, m_cascadeTime - dt);
            }
        }
        float fx = 0.0f, fz = 0.0f;
        if (!climbs || companionDown() || m_engaged || m_cascadeRest > 0.0f || companion.changing() ||
            !habitats.nearestCascade(companion.body.position.x, companion.body.position.z, CASCADE_SEEK, fx, fz)) return nullptr;
        m_cascadeGoal = { fx, companion.body.position.y, fz };
        if (!Geometry::within(m_cascadeGoal, companion.body.position, CASCADE_RANGE)) return &m_cascadeGoal; // todavía va hacia allí
        float tx = fx, ty = 0.0f, tz = fz;
        if (!habitats.cascadeTop(fx, fz, CASCADE_TOP_REACH, tx, ty, tz)) { m_cascadeRest = CASCADE_REST_MIN; return nullptr; }
        m_cascadeRest = RandomUtil::range(CASCADE_REST_MIN, CASCADE_REST_MAX);
        companion.beginClimb({ tx, ty + 0.1f, tz }, RandomUtil::range(0.0f, 1.0f) < CASCADE_SUCCESS, RandomUtil::range(0.35f, 0.75f));
        return nullptr;
    }

    // El líder del equipo acompaña al jugador y trabaja solo lo cercano: recoge plantas crecidas y objetos sueltos (cualquier
    // pokémon) y trabaja los nodos y riega las plantas si su habilidad del mundo llega al nivel que piden.
    void updateCompanion(float dt) {
        if (m_respawn <= 0.0f && storage.active() && storage.fainted(*storage.active()) && !companion.changing()) { // el compañero cae: vuelve a su bola y sale el siguiente
            const int next = storage.nextUsable();
            if (next >= 0) {
                const PokemonSpecies* fallen = m_data->speciesById(storage.active()->speciesId);
                storage.setActive(next);
                showNotice(Notice::REWARD, (fallen ? fallen->name : std::string("Tu pokémon")) + " está debilitado");
            }
        }
        const OwnedPokemon* lead = storage.active();
        const PokemonSpecies* species = lead ? m_data->speciesById(lead->speciesId) : nullptr;
        if (!lead && companion.active()) companion.set(-1, false, false, player.body.position, camera.yaw());
        else if (lead && species && (lead->uid != m_shownUid || (lead->speciesId != m_shownSpecies && lead->uid != m_itemEvolveUid))) {
            m_shownUid = lead->uid; // otro pokémon (o ha evolucionado): vuelve el que está fuera y sale su pokéball
            m_shownSpecies = lead->speciesId;
            companion.swap(species->id, m_data->levitates(*species), lead->ballId, lead->shiny, player.body.position, camera.yaw());
        }
        if (m_itemEvolveUid != 0 && lead && lead->uid == m_itemEvolveUid && companion.active() && !companion.changing() && !companion.tripActive()) {
            if (companion.speciesId() == lead->speciesId) m_itemEvolveUid = 0; // ya sale con la nueva especie: solo brilla
            companion.beginEvolve(); // el destello cambia la especie (updateWaterfall) y entonces termina la evolución por objeto
        }
        if (companion.changing() || companion.tripActive()) species = nullptr; // durante el cambio o el intento de subir la cascada no trabaja
        updateDuel(dt, lead, species);
        if (companionDown()) species = nullptr; // debilitado: solo sigue al jugador hasta que lo reviva un objeto o el centro pokémon

        // Busca lo más cercano que sabe trabajar dentro de su radio de búsqueda; si está lejos, va hacia ello (aunque se separe
        // del jugador) y, al llegar, lo trabaja. Sin nada que hacer vuelve a seguir al jugador.
        ResourceNode* nodeTarget = nullptr;
        GroundItem* groundTarget = nullptr;
        Fruit* fruitTarget = nullptr;
        int level = 1;
        float best = 1.0e9f;
        const auto distance2 = [&](const DirectX::XMFLOAT3& at) { return Geometry::distance2XZ(at, companion.body.position); };
        const auto inRange = [&](const DirectX::XMFLOAT3& at) { return nearPlayer(at, Companion::SEARCH_RANGE); };
        const CompanionRules::Mode mode = m_duelTrainer >= 0 ? CompanionRules::Mode::BATTLE : storage.mode; // en un duelo siempre pelea
        tickCollectWindow(dt);
        // Lo que ya empezó (un árbol a medio talar, una roca a medio picar) lo termina aunque se cierre la ventana de recolección, pero no encadena el siguiente.
        if (m_taskNode >= 0) {
            ResourceNode& task = nodes[static_cast<size_t>(m_taskNode)];
            const ResourceNodeType& type = m_data->nodes[task.typeIndex()];
            if (species && mode == CompanionRules::Mode::COLLECT && task.workable() && !task.plant() && task.hits() > 0 && inRange(task.body.position)) {
                nodeTarget = &task;
                best = distance2(task.body.position);
                level = (std::max)(1, species->levelIn(type.skillId));
            } else m_taskNode = -1;
        }
        if (!nodeTarget && species && mode == CompanionRules::Mode::COLLECT && m_collecting) {
            for (ResourceNode& node : nodes) {
                if (!node.canWork(false) || !inRange(node.body.position)) continue;
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
                    fruitTarget = nullptr;
                    nodeTarget = nullptr;
                    level = 1;
                }
            }
            for (Fruit& fruit : fruits) { // también recoge los frutos caídos (el jugador tiene prioridad: si los coge antes, ya no están)
                if (!fruit.available() || !fruit.body.onGround || !inRange(fruit.body.position)) continue;
                const float d2 = distance2(fruit.body.position);
                if (d2 < best) {
                    best = d2;
                    fruitTarget = &fruit;
                    groundTarget = nullptr;
                    nodeTarget = nullptr;
                    level = 1;
                }
            }
        }
        m_captureRest = (std::max)(0.0f, m_captureRest - dt);
        // Ataca al mismo salvaje hasta terminar con él (en captura, hasta dejarlo en el punto de captura rápida); el descanso
        // antes de ir a por otro empieza entonces, no tras cada golpe.
        if (m_engaged && (mode == CompanionRules::Mode::COLLECT || !engagedValid(mode))) {
            m_engaged = nullptr;
            if (mode != CompanionRules::Mode::COLLECT) m_captureRest = RandomUtil::range(CompanionRules::CAPTURE_REST_MIN, CompanionRules::CAPTURE_REST_MAX);
        }
        const bool resting = m_duelTrainer < 0 && !m_engaged && m_captureRest > 0.0f; // (en un duelo no descansa: combate sin pausa) // entre combates sigue al jugador
        CaptureTarget* fightTarget = nullptr;
        if (species && mode != CompanionRules::Mode::COLLECT && !resting) {
            fightTarget = m_engaged ? m_engaged : (m_duelTrainer >= 0 && wild.foe() && wild.foe()->hittable() ? wild.foe() : fightTargetFor(mode));
            m_engaged = fightTarget;
        }
        if (fightTarget) best = distance2(fightTarget->body.position);
        const DirectX::XMFLOAT3* fallGoal = (!nodeTarget && !groundTarget && !fruitTarget && !fightTarget) ? updateWaterfall(dt, species) : nullptr;
        const DirectX::XMFLOAT3* goal = nodeTarget ? &nodeTarget->body.position : groundTarget ? &groundTarget->body.position : fruitTarget ? &fruitTarget->body.position :
                                        fightTarget ? &fightTarget->body.position : fallGoal;
        const bool ranged = species && DamageRules::styleOf(*species) == DamageRules::Style::RANGED;
        const float fightReach = ranged ? CompanionRules::RANGED_RANGE : CompanionRules::ATTACK_RANGE;
        const float reach = fallGoal ? CASCADE_RANGE : nodeTarget ? Companion::WORK_RANGE + ResourceStyle::look(nodeTarget->typeId()).half :
                            fightTarget ? fightReach : Companion::WORK_RANGE;
        const float approach = fightTarget && ranged ? fightReach * CompanionRules::RANGED_APPROACH : Companion::APPROACH;
        bool arrived = goal && best <= reach * reach;
        if (arrived && fightTarget && ranged) arrived = !world.blocked(companion.center(), fightTarget->center()); // sin roca de por medio
        if (arrived && fightTarget && !ranged) arrived = best <= fightReach * fightReach;
        if (nodeTarget && arrived) nodeTarget->companionWorking();
        const float teamBonus = nodeTarget && arrived ? nodeTarget->teamBonus(false) : 1.0f;
        const float power = arrived ? Companion::workPower(fightTarget ? 1 : level, teamBonus) : 0.0f;
        const bool tending = potionFx.time >= 0.0f && potionFx.time < POTION_SPRAY_TO; // viene a donde el jugador le está curando
        if (!companion.update(dt, world, player.body.position, camera.yaw(), tending ? 0.0f : power, tending ? &player.body.position : goal, tending ? POTION_DISTANCE : approach, fightTarget != nullptr && !tending)) return;
        if (nodeTarget) workNode(*nodeTarget, false);
        else if (groundTarget) collect(*groundTarget);
        else if (fruitTarget) collectFruit(*fruitTarget);
        else if (fightTarget) {
            if (m_duelTurn == DuelTurn::COMPANION) { // combate por turnos: pega uno y luego el otro, así ninguno se pasa la pelea huyendo del otro
                strike(*fightTarget);
                const HeldItem* held = leadHeld();
                const bool again = held && held->effect == HeldItem::Effect::QUICK_CLAW && chance(held->amount); // Garra rápida: vuelve a pegar antes de que responda
                m_duelTurn = again ? DuelTurn::COMPANION : DuelTurn::WILD;
                m_turnClock = 0.0f;
            }
        }
    }

    // Combate por turnos contra el salvaje al que ataca el acompañante: tras cada golpe suyo responde el salvaje (de cerca si es físico, de lejos si
    // es especial) con el mismo cálculo (tipos y estadísticas) contra la vida del acompañante. Si este cae, descansa un rato y se recupera.
    // Cura al compañero (aguas termales...). Uno debilitado (0 PS) no se cura con esto: solo reviviéndolo (objeto o centro pokémon).
    void healCompanion(float fraction) {
        OwnedPokemon* lead = storage.activeMut();
        if (!lead || storage.fainted(*lead)) return;
        m_healAcc += fraction * static_cast<float>(storage.maxHp(*lead));
        if (m_healAcc >= 1.0f) { const int whole = static_cast<int>(m_healAcc); m_healAcc -= static_cast<float>(whole); storage.heal(true, storage.activeIndex(), whole); }
    }
    bool companionDown() const { const OwnedPokemon* lead = storage.active(); return lead && storage.fainted(*lead); }
    float companionHpFraction() const { const OwnedPokemon* lead = storage.active(); return lead ? storage.hpFraction(*lead) : 1.0f; }

    void updateDuel(float dt, const OwnedPokemon* lead, const PokemonSpecies* species) {
        if (!lead) { m_engaged = nullptr; return; }
        OwnedPokemon* mine = storage.activeMut();
        if (storage.fainted(*mine)) { // debilitado: no se cura solo (hace falta revivirlo)
            if (m_engaged) { m_engaged->stopFight(); m_engaged = nullptr; }
            return;
        }
        if (!m_engaged || !species || companion.changing()) {
            m_duelTurn = DuelTurn::COMPANION;
            return;
        }
        CaptureTarget& foe = *m_engaged;
        const PokemonSpecies* attacker = m_data->speciesById(foe.speciesId());
        if (!attacker) return;
        const bool ranged = DamageRules::styleOf(*attacker) == DamageRules::Style::RANGED;
        const float keep = ranged ? CompanionRules::RANGED_RANGE * 0.7f : CompanionRules::ATTACK_RANGE * 0.7f;
        if (m_duelTurn != DuelTurn::WILD) { foe.fightToward(companion.center(), keep); return; } // espera su turno encarándolo
        m_turnClock += dt;
        foe.fightToward(companion.center(), keep);
        const float d2 = Geometry::distance2XZ(foe.body.position, companion.body.position);
        const float reach = ranged ? CompanionRules::RANGED_RANGE : CompanionRules::ATTACK_RANGE;
        const bool inReach = d2 <= reach * reach && (!ranged || !world.blocked(foe.center(), companion.center()));
        if (m_turnClock >= WILD_TURN_DELAY && inReach) {
            const DamageRules::Hit hit = DamageRules::compute(*m_data, *attacker, foe.level(), *species, lead->level, foe.hpFraction());
            if (hit.lands()) {
                strikes.push_back({ foe.center(), companion.center(), hit.typeId, hit.style == DamageRules::Style::RANGED, 0.0f });
                int damage = (std::max)(1, static_cast<int>(hit.fraction * WILD_DAMAGE_SCALE * static_cast<float>(storage.maxHp(*mine))));
                const HeldItem* held = storage.heldOf(*mine);
                if (held && held->effect == HeldItem::Effect::FOCUS_BAND && damage >= storage.hpOf(*mine) && storage.hpOf(*mine) > 1 && chance(held->amount)) {
                    damage = storage.hpOf(*mine) - 1; // Cinta focus: aguanta con 1 PS
                    showNotice(Notice::REWARD, species->name + " aguantó con la Cinta focus");
                }
                storage.hurt(*mine, damage);
            }
            m_duelTurn = DuelTurn::COMPANION;
        } else if (m_turnClock >= WILD_TURN_TIMEOUT) {
            m_duelTurn = DuelTurn::COMPANION; // no llega a tiempo: pasa el turno
        }
        if (storage.fainted(*mine)) {
            foe.stopFight();
            m_engaged = nullptr;
            m_duelTurn = DuelTurn::COMPANION;
            showNotice(Notice::REWARD, species->name + " está agotado");
        }
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
        return nearestWild(Companion::SEARCH_RANGE, [&](const CaptureTarget& target) {
            if (mode == CompanionRules::Mode::CAPTURE && target.hpFraction() <= CompanionRules::CAPTURE_STOP_HP) return false;
            return hitOn(target).lands(); // inmune a sus tipos: no lo ataca
        });
    }

    // El salvaje cargado más cercano al jugador, dentro de 'range', en su misma capa (suelo o galería), con especie y que no sea el pescado
    // (ese es del jugador), que además cumpla 'accept'.
    template <typename Accept>
    CaptureTarget* nearestWild(float range, Accept accept) const {
        CaptureTarget* best = nullptr;
        float bestDistance = range * range;
        for (CaptureTarget* target : wild.loaded()) {
            if (!target->hittable() || target->speciesIndex() < 0 || wild.isHooked(target) || target->body.underground != player.body.underground || !accept(*target)) continue;
            const float d2 = Geometry::distance2XZ(target->body.position, player.body.position);
            if (d2 < bestDistance) {
                bestDistance = d2;
                best = target;
            }
        }
        return best;
    }

    // ¿Sigue siendo válido el salvaje con el que está peleando el acompañante?
    bool engagedValid(CompanionRules::Mode mode) const {
        for (CaptureTarget* target : wild.loaded()) {
            if (target != m_engaged) continue;
            if (!target->hittable() || wild.isHooked(target) || target->body.underground != player.body.underground) return false;
            return !(mode == CompanionRules::Mode::CAPTURE && target->hpFraction() <= CompanionRules::CAPTURE_STOP_HP);
        }
        return false; // descargado o capturado
    }

    // ¿Ve el jugador ese punto? No si hay terreno (montaña, loma, entrada de cueva) o un sólido de por medio.
    bool canSee(float x, float y, float z) const {
        const DirectX::XMFLOAT3 eye = { player.body.position.x, player.body.position.y + 1.5f, player.body.position.z };
        const float dx = x - eye.x, dy = y - eye.y, dz = z - eye.z;
        const int steps = (std::min)(40, (std::max)(2, static_cast<int>(std::sqrt(dx * dx + dz * dz) / 3.0f)));
        for (int i = 1; i < steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            if (world.terrain.sample(eye.x + dx * t, eye.z + dz * t) > eye.y + dy * t + 0.3f) return false;
        }
        return !world.blocked(eye, { x, y, z });
    }

    // Lo que el líder del equipo le haría a un salvaje de un golpe (DamageRules); sin líder o sin datos, nada.
    DamageRules::Hit hitOn(const CaptureTarget& target) const {
        const OwnedPokemon* lead = storage.active();
        const PokemonSpecies* attacker = lead ? m_data->speciesById(lead->speciesId) : nullptr;
        const PokemonSpecies* defender = m_data->speciesById(target.speciesId());
        if (!attacker || !defender) return {};
        return DamageRules::compute(*m_data, *attacker, lead->level, *defender, target.level());
    }

    static bool chance(int percent) { return RandomUtil::range(0.0f, 100.0f) < static_cast<float>(percent); }
    const HeldItem* leadHeld() const { const OwnedPokemon* lead = storage.active(); return lead ? storage.heldOf(*lead) : nullptr; }

    // Repartir experiencia: los demás del equipo que lo llevan (y no están debilitados) se llevan un % de la experiencia del combate.
    void shareExperience(int xp) {
        for (int i = 0; i < static_cast<int>(storage.team().size()); ++i) {
            if (i == storage.activeIndex()) continue;
            const OwnedPokemon& member = storage.team()[i];
            const HeldItem* held = storage.heldOf(member);
            if (!held || held->effect != HeldItem::Effect::EXP_SHARE || storage.fainted(member)) continue;
            storage.addXp(i, (std::max)(1, xp * held->amount / 100), progress.levelCap());
        }
    }

    // Restos: el equipo que los lleva recupera PS poco a poco, también fuera de combate.
    void updateHeldItems(float dt) {
        if ((m_leftoversTimer -= dt) > 0.0f) return;
        m_leftoversTimer = LEFTOVERS_INTERVAL;
        for (int i = 0; i < static_cast<int>(storage.team().size()); ++i) {
            const OwnedPokemon& member = storage.team()[i];
            const HeldItem* held = storage.heldOf(member);
            if (held && held->effect == HeldItem::Effect::LEFTOVERS && !storage.fainted(member)) storage.heal(true, i, (std::max)(1, storage.maxHp(member) * held->amount / 100));
        }
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
        float fraction = hit.fraction;
        if (const HeldItem* held = leadHeld()) {
            if (held->effect == HeldItem::Effect::RAZOR_CLAW && chance(held->amount)) { fraction *= CRITICAL_MULTIPLIER; showNotice(Notice::REWARD, "¡Golpe crítico!"); }
            else if (held->effect == HeldItem::Effect::LIFE_ORB) {
                fraction *= 1.0f + static_cast<float>(held->amount) / 100.0f;
                if (OwnedPokemon* mine = storage.activeMut()) storage.hurt(*mine, (std::max)(1, storage.maxHp(*mine) / LIFE_ORB_RECOIL_DIVISOR)); // la Vidasfera cobra su precio
            }
        }
        target.addAttacker(storage.activeIndex());
        if (!target.damage((std::min)(fraction, DamageRules::MAX_HIT))) return;

        // Derrotado: la experiencia (fórmula única, ExperienceRules) se reparte entre quienes lo golpearon y siguen en pie.
        std::vector<int> winners;
        for (const int index : target.attackers) if (index >= 0 && index < static_cast<int>(storage.team().size()) && !storage.fainted(storage.team()[static_cast<size_t>(index)])) winners.push_back(index);
        if (std::find(winners.begin(), winners.end(), storage.activeIndex()) == winners.end()) winners.push_back(storage.activeIndex());
        float rarity = 1.0f;
        if (const PokemonSpecies* foe = m_data->speciesById(target.speciesId())) rarity = m_data->xpMultiplier(*foe);
        std::string text;
        int activeXp = 0;
        PokemonStorage::XpResult activeResult;
        for (const int index : winners) {
            const int xp = storage.team()[static_cast<size_t>(index)].level >= progress.levelCap() ? 0 : ExperienceRules::pokemonXp(target.level(), storage.team()[static_cast<size_t>(index)].level, static_cast<int>(winners.size()), rarity);
            const PokemonStorage::XpResult result = storage.addXp(index, xp, progress.levelCap());
            if (index == storage.activeIndex()) { activeXp = xp; activeResult = result; }
            if (result.evolved) Sound::emit(Sound::Sfx::EVOLVE, companion.center());
        }
        shareExperience(activeXp);
        if (m_duelTrainer >= 0) { // contra un entrenador, el jugador también gana experiencia
            const int mine = ExperienceRules::playerXp(target.level(), progress.level());
            if (progress.addXp(mine) > 0) text = "¡Nivel " + std::to_string(progress.level()) + "!  ";
        }
        if (activeXp > 0) text += name + "  +" + std::to_string(activeXp) + " PX";
        if (activeResult.levels > 0) text += "   ¡Nivel " + std::to_string(storage.active()->level) + "!";
        showNotice(Notice::REWARD, std::move(text));
    }

    // --- Interacción (cofres y recursos) ---
    // Lo interactuable más cercano de la lista que esté al alcance del jugador: devuelve su índice y deja la distancia² en best.
    template <typename Entity, typename Usable>
    int nearestIn(const std::vector<Entity>& list, Usable usable, float& best) const {
        int index = -1;
        for (size_t i = 0; i < list.size(); ++i) {
            if (!usable(list[i]) || list[i].body.underground != player.body.underground) continue; // bajo tierra y en la superficie no se alcanzan
            const float d2 = Geometry::distance2XZ(list[i].body.position, player.body.position);
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

    // El aviso de capturar al debilitado dura WEAK_PROMPT_TIME desde que aparece; con otro debilitado vuelve a empezar.
    void updateWeakTimer(float dt) {
        const CaptureTarget* now = weakenedTarget();
        if (now != m_weakSeen) { m_weakSeen = now; m_weakAge = 0.0f; m_weakExpired = false; }
        else if (now && (m_weakAge += dt) >= CompanionRules::WEAK_PROMPT_TIME) m_weakExpired = true;
    }

    void updateInteraction() {
        float best = 1.0e9f;
        m_interaction = {};
        m_missingSkill = -1;
        m_weak = m_weakExpired ? nullptr : weakenedTarget();
        if (const int i = nearestIn(chests.entities, [](const Chest& c) { return c.closed(); }, best); i >= 0) m_interaction = { Interaction::CHEST, i };
        if (const int i = nearestIn(groundItems.entities, [](const GroundItem& g) { return g.available(); }, best); i >= 0) m_interaction = { Interaction::GROUND, i };
        if (const int i = nearestIn(fruits, [](const Fruit& f) { return f.available(); }, best); i >= 0) m_interaction = { Interaction::FRUIT, i };
        const float mx = machine.body.position.x - player.body.position.x, mz = machine.body.position.z - player.body.position.z;
        if (const float d2 = mx * mx + mz * mz; d2 <= machine.reach() * machine.reach() && d2 < best) {
            best = d2;
            m_interaction = { Interaction::MACHINE, 0 };
        }
        const float ox = oak.body.position.x - player.body.position.x, oz = oak.body.position.z - player.body.position.z;
        if (const float d2 = ox * ox + oz * oz; d2 <= oak.reach() * oak.reach() && d2 < best) {
            best = d2;
            m_interaction = { Interaction::PROFESSOR, 0 };
        }
        for (int i = 0; i < static_cast<int>(trainers.size()); ++i) {
            if (trainers[i].busy || battle) continue;
            const float tx = trainers[i].body.position.x - player.body.position.x, tz = trainers[i].body.position.z - player.body.position.z, d2 = tx * tx + tz * tz;
            if (d2 <= trainers[i].reach() * trainers[i].reach() && d2 < best) { best = d2; m_interaction = { Interaction::TRAINER, i }; }
        }
        if (const int i = nearestIn(nodes, [](const ResourceNode& n) { return n.canWork(true); }, best); i >= 0) {
            const ResourceNode& node = nodes[i];
            if (playerCanWork(node)) m_interaction = { Interaction::NODE, i };
            else {
                const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
                m_missingSkill = type.skillId; // sin herramienta de ese nivel: solo un pokémon que llegue al nivel puede trabajarlo
                m_missingLevel = type.level;
                m_missingPos = nodeAnchor(node);
            }
        }
        if (availableSeeds().empty()) m_seedCrop = -1;
        else for (int i = 0; i < static_cast<int>(plots.size()); ++i) { // parcela vacía y con alguna semilla: se puede sembrar lo que se elija
            if (!nodes[plots[i].node].gone()) continue;
            const float dx = plots[i].position.x - player.body.position.x, dz = plots[i].position.z - player.body.position.z, d2 = dx * dx + dz * dz;
            if (d2 <= Crops::REACH * Crops::REACH && d2 < best) { best = d2; m_interaction = { Interaction::PLOT, i }; }
        }
        if (m_weak) m_interaction = { Interaction::WEAKENED, 0 }; // capturar al debilitado tiene prioridad
    }

    public:
    // Material de las semillas de un cultivo, y esas semillas como objeto del inventario.
    const Material* seedMaterial(int crop) const {
        int materialId = -1;
        if (!Crops::LIST[crop].seedName) { // se siembra el propio producto: el material que suelta el cultivo
            for (const ResourceNodeType& type : m_data->nodes) if (type.id == Crops::LIST[crop].nodeId) materialId = type.materialId;
            for (const Material& material : m_data->materials) if (material.id == materialId) return &material;
            return nullptr;
        }
        for (const Material& material : m_data->materials) if (material.name == Crops::LIST[crop].seedName) return &material;
        return nullptr;
    }
    const Item* seedItem(int crop) const {
        const Material* material = seedMaterial(crop);
        return material ? m_data->materialItem(material->id) : nullptr;
    }

    // Cultivos de los que el jugador tiene semillas.
    std::vector<int> availableSeeds() const {
        std::vector<int> crops;
        for (int crop = 0; crop < Crops::COUNT; ++crop) {
            const Item* seeds = seedItem(crop);
            if (seeds && inventory.count(seeds->id) > 0 && Crops::typeIndexOf(*m_data, crop) >= 0) crops.push_back(crop);
        }
        return crops;
    }

    // Semillas que el jugador puede sembrar (para la pantalla de elección).
    struct SeedChoice { int crop; const Item* item; std::string name; int count; };
    std::vector<SeedChoice> seedChoices() const {
        std::vector<SeedChoice> choices;
        for (const int crop : availableSeeds()) {
            const Item* item = seedItem(crop);
            choices.push_back({ crop, item, item->name, inventory.count(item->id) });
        }
        return choices;
    }

    // Siembra en la parcela pendiente las semillas elegidas en la pantalla de elección.
    void plantSeed(int crop) {
        if (m_pendingPlot >= 0 && m_pendingPlot < static_cast<int>(plots.size())) {
            m_seedCrop = crop;
            sow(plots[m_pendingPlot]);
        }
        m_pendingPlot = -1;
    }

    private:
    void sow(const Crops::Plot& plot) {
        const std::vector<int> seeds = availableSeeds();
        if (std::find(seeds.begin(), seeds.end(), m_seedCrop) == seeds.end()) return;
        ResourceNode& node = nodes[plot.node];
        const Item* item = seedItem(m_seedCrop);
        if (!node.gone()) return;
        const int typeIndex = Crops::typeIndexOf(*m_data, m_seedCrop);
        const ResourceNodeType& type = m_data->nodes[typeIndex];
        inventory.add(item->id, -1);
        node.sow(typeIndex, type.id, type.hits, static_cast<float>(type.growSeconds), RandomUtil::range(0.0f, 6.2831853f));
        player.act(PlayerAction::PICK);
        Sound::emit(Sound::Sfx::PICK, node.body.position);
    }

    // En modo captura, el salvaje más cercano que el acompañante ha dejado debilitado (con la vida en o bajo CAPTURE_STOP_HP) y que se
    // puede capturar ya: aparece la ayuda de interacción "Capturar" sobre él.
    const CaptureTarget* weakenedTarget() const {
        if (storage.mode != CompanionRules::Mode::CAPTURE || !storage.active() || fishing.active() || m_aiming) return nullptr;
        return nearestWild(CompanionRules::WEAK_REACH, [](const CaptureTarget& target) { return target.hpFraction() <= CompanionRules::CAPTURE_STOP_HP; });
    }

    void interact() {
        m_interaction = validInteraction();
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
        } else if (m_interaction.kind == Interaction::FRUIT) {
            player.act(PlayerAction::PICK);
            collectFruit(fruits[m_interaction.index]);
        } else if (m_interaction.kind == Interaction::MACHINE) researchRequested = true;
        else if (m_interaction.kind == Interaction::PROFESSOR) {
            if (needsStarter()) starterRequested = true;
            else showNotice(Notice::REWARD, std::string(Professor::NAME) + ": ¡Cuida bien de tu compañero!");
        }
        else if (m_interaction.kind == Interaction::TRAINER) {
            if (trainers[m_interaction.index].style == Trainer::Style::SHOP) shopRequested = true;
            else startDuel(m_interaction.index);
        }
        else if (m_interaction.kind == Interaction::PLOT) {
            const std::vector<int> seeds = availableSeeds();
            if (seeds.size() == 1) { m_seedCrop = seeds.front(); sow(plots[m_interaction.index]); }
            else if (seeds.size() > 1) { m_pendingPlot = m_interaction.index; seedRequested = true; } // varias semillas: la interfaz deja elegir
        }
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

    // Entrega una entrada de recompensa: unidades de un objeto o material al inventario.
    void giveEntry(const RewardEntry& entry) {
        if (const Item* item = m_data->rewardItem(entry)) giveReward(item->id, RewardRules::quantity(entry));
        else if (entry.kind == RewardEntry::Kind::XP) gainXp(RewardRules::quantity(entry));
    }

    // Abre el cofre: una recompensa al azar de su grupo, directa al inventario.
    void openChest(Chest& chest) {
        const RewardEntry* reward = RewardRules::pick(*m_data, m_data->chests[chest.typeIndex()].rewardId);
        chest.open();
        Sound::emit(Sound::Sfx::CHEST_OPEN, chest.body.position);
        if (reward) giveEntry(*reward);
        gainXp(Xp::CHEST);
    }

    // Un paso de trabajo en el nodo (riego o golpe); al agotarlo da su material.
    // Sonido de trabajar un nodo: depende de lo que es (roca: picar; árbol: talar; planta: recoger), no de quién lo trabaja.
    static void nodeSound(const ResourceNode& node) {
        using Shape = ResourceStyle::Shape;
        const Shape shape = ResourceStyle::look(node.typeId()).shape;
        const Sound::Sfx sfx = shape == Shape::ROCK ? Sound::Sfx::MINE : ResourceStyle::isTree(shape) ? Sound::Sfx::CHOP : Sound::Sfx::PICK;
        Sound::emit(sfx, node.body.position);
    }

    void workNode(ResourceNode& node, bool byPlayer, float speed = 1.0f) {
        if (!node.work(byPlayer, speed)) return;
        nodeSound(node);
        if (!byPlayer && !node.plant() && node.hits() > 0) m_taskNode = static_cast<int>(&node - nodes.data()); // el pokémon ya empezó: lo termina
        if (!node.takeHarvest()) return;
        if (!byPlayer && m_taskNode == static_cast<int>(&node - nodes.data())) { // terminó: descansa antes de ir a por otra cosa
            m_taskNode = -1;
            m_collecting = false;
            m_collectClock = RandomUtil::range(COLLECT_WAIT_MIN, COLLECT_WAIT_MAX);
        }
        const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
        if (const Item* item = m_data->materialItem(type.materialId)) giveReward(item->id, RandomUtil::integer(type.minYield, type.maxYield));
        gainXp(Xp::GATHER);
    }

    // --- Frutos: cocos de palmera, plátanos del árbol de jungla y cerezas del cerezo ---
    static constexpr int   MAX_FRUITS = 20, MAX_PER_KIND = 5; // como mucho en el suelo a la vez (en total y de cada clase: así los cocos no tapan a los plátanos)
    static constexpr float FRUIT_DROP_MIN = 7.0f, FRUIT_DROP_MAX = 16.0f; // segundos entre caídas
    static constexpr float FRUIT_RANGE = 55.0f;      // solo caen en árboles cercanos al jugador
    static constexpr int   PALM_TYPE = 10, JUNGLE_TYPE = 13, CHERRY_TYPE = 11;

    inline static const std::string COCONUT_NAME = "Coco", HALF_COCONUT_NAME = "Mitad de coco", BANANA_NAME = "Plátano", BANANAS_NAME = "Plátanos", BUNCH_NAME = "Racimo de plátanos", CHERRY_NAME = "Cereza";
    static const std::string& fruitName(const Fruit& fruit) {
        if (fruit.half()) return HALF_COCONUT_NAME; // el coco partido es otro objeto (material "Mitad de coco")
        if (fruit.bunch()) return fruit.bananaCount() >= Fruit::BUNCH_SIZE ? BUNCH_NAME : fruit.bananaCount() > 1 ? BANANAS_NAME : BANANA_NAME; // el racimo entero es un objeto; si le faltan, se cogen sueltos
        return fruit.kind() == Fruit::Kind::COCONUT ? COCONUT_NAME : fruit.kind() == Fruit::Kind::CHERRY ? CHERRY_NAME : BANANA_NAME;
    }
    static bool fruitKind(int treeType, Fruit::Kind& kind) {
        if (treeType == PALM_TYPE) kind = Fruit::Kind::COCONUT;
        else if (treeType == JUNGLE_TYPE) kind = Fruit::Kind::BANANA;
        else if (treeType == CHERRY_TYPE) kind = Fruit::Kind::CHERRY;
        else return false;
        return true;
    }

    const Item* fruitItem(const Fruit& fruit) const {
        const std::string& name = fruit.bunch() ? (fruit.bananaCount() >= Fruit::BUNCH_SIZE ? BUNCH_NAME : BANANA_NAME) : fruitName(fruit);
        for (const Material& material : m_data->materials) {
            if (material.name == name) return m_data->materialItem(material.id);
        }
        return nullptr;
    }

    // El jugador o su pokémon cogen un fruto: va al inventario.
    void collectFruit(Fruit& fruit) {
        if (!fruit.available()) return;
        const int amount = fruit.bunch() && fruit.bananaCount() < Fruit::BUNCH_SIZE ? fruit.bananaCount() : 1; // un racimo entero es un objeto; a medias, tantos plátanos como queden
        if (const Item* item = fruitItem(fruit)) giveReward(item->id, amount);
        gainXp(Xp::PICK_UP);
        fruit.take();
        Sound::emit(Sound::Sfx::PICK, fruit.body.position); // el sonido es del fruto: el mismo para quien lo coja
    }

    static constexpr float COCONUT_BREAK_CHANCE = 12.0f; // % de cocos que se parten al caer del árbol

    // El coco entero se parte en sus dos mitades (cada una un cuerpo físico distinto que sale despedido en sentido contrario).
    // 'eaten': un pokémon se las come, así que se encogen despacio, una tras otra.
    void breakCoconut(Fruit& whole, bool eaten) {
        const DirectX::XMFLOAT3 at = whole.body.position, carried = whole.body.velocity; // copias: añadir frutos invalida 'whole'
        const float angle = RandomUtil::range(0.0f, 6.2831853f), push = eaten ? 0.25f : RandomUtil::range(0.9f, 1.6f), lift = eaten ? 0.8f : RandomUtil::range(1.4f, 2.2f);
        whole.consume();
        Sound::emit(Sound::Sfx::COCONUT_BREAK, at);
        for (int i = 0; i < 2; ++i) {
            const float side = i == 0 ? 1.0f : -1.0f;
            fruits.emplace_back(Fruit::Kind::COCONUT, ++m_fruitId, DirectX::XMFLOAT3{ at.x, at.y + 0.04f, at.z },
                                std::sin(angle) * push * side + carried.x * 0.3f, std::cos(angle) * push * side + carried.z * 0.3f,
                                i == 0 ? Fruit::Part::HALF_A : Fruit::Part::HALF_B, lift);
            if (eaten) fruits.back().take(1.4f + 0.6f * static_cast<float>(i), 0.5f + 0.4f * static_cast<float>(i));
        }
    }

    static constexpr float BANANA_DETACH_CHANCE = 30.0f; // % de plátanos que se sueltan del racimo al caer del árbol

    // Un plátano se separa del racimo y queda como pieza física suelta ('eaten': se lo come un pokémon, así que se encoge despacio).
    // OJO: añade frutos al vector, así que 'bunch' no vale después.
    void detachBanana(Fruit& bunch, int variant, bool eaten) {
        const DirectX::XMFLOAT3 at = bunch.body.position, carried = bunch.body.velocity;
        const float angle = RandomUtil::range(0.0f, 6.2831853f), push = eaten ? 0.0f : RandomUtil::range(0.5f, 1.4f), lift = eaten ? 0.5f : RandomUtil::range(1.0f, 2.0f);
        if (bunch.bananaCount() == 0) bunch.take(eaten ? 0.6f : 0.0f);
        fruits.emplace_back(Fruit::Kind::BANANA, ++m_fruitId, DirectX::XMFLOAT3{ at.x, at.y + 0.05f, at.z },
                            std::sin(angle) * push + carried.x * 0.3f, std::cos(angle) * push + carried.z * 0.3f, Fruit::Part::PIECE, lift, variant);
        if (eaten) fruits.back().take(1.4f, 0.2f);
    }

    // Un pokémon come un plátano del racimo: se quita uno solo; cuando no queda ninguno, el racimo desaparece.
    void eatBanana(Fruit& bunch) {
        if (bunch.bananaCount() == 0) { bunch.take(); return; }
        const int variant = bunch.removeBanana(RandomUtil::integer(0, bunch.bananaCount() - 1));
        detachBanana(bunch, variant, true);
    }

    // El racimo acaba de caer del árbol: cada plátano tiene su probabilidad de soltarse (unos racimos pierden más que otros).
    void shedBananas(int bunchId) {
        for (size_t i = 0; i < fruits.size(); ++i) {
            if (fruits[i].id() != bunchId) continue;
            std::vector<int> lost;
            for (int b = 0; b < Fruit::BUNCH_SIZE; ++b) if (fruits[i].hasBanana(b) && RandomUtil::roll(BANANA_DETACH_CHANCE)) lost.push_back(b);
            for (const int b : lost) {
                fruits[i].removeBanana(bananaRank(fruits[i], b));
                detachBanana(fruits[i], b, false); // puede invalidar fruits[i]: se vuelve a tomar en cada vuelta
                for (size_t j = 0; j < fruits.size(); ++j) if (fruits[j].id() == bunchId) { i = j; break; }
            }
            return;
        }
    }
    static int bananaRank(const Fruit& bunch, int variant) { int n = 0; for (int i = 0; i < variant; ++i) n += bunch.hasBanana(i) ? 1 : 0; return n; }

    // Un salvaje se come el fruto: suena como cogerlo y, además, como comerlo (cosas del fruto, no del pokémon).
    // Un coco entero primero se parte; las mitades desaparecen mientras se lo come.
    // OJO: puede añadir frutos al vector, así que 'fruit' no vale tras llamarla.
    void eatFruit(Fruit& fruit) {
        const DirectX::XMFLOAT3 at = fruit.body.position;
        if (fruit.kind() == Fruit::Kind::COCONUT && !fruit.half()) breakCoconut(fruit, true);
        else if (fruit.bunch()) eatBanana(fruit);
        else fruit.take();
        Sound::emit(Sound::Sfx::PICK, at);
        Sound::emit(Sound::Sfx::EAT, at);
    }

    // De vez en cuando cae un fruto de un árbol maduro cercano (si no hay ya demasiados en el suelo) y rueda con su inercia.
    void updateFruits(float dt) {
        std::vector<int> cracking, shedding; // cocos que acaban de caer y se rompen / racimos que pierden plátanos (tras el bucle: añaden frutos)
        for (size_t i = 0; i < fruits.size(); ++i) {
            Fruit& fruit = fruits[i];
            world.step(fruit.body, dt);
            fruit.update(dt);
            if (const float landing = fruit.takeLanding(); landing > 0.0f) {
                if (fruit.breaksOnLanding()) cracking.push_back(fruit.id());
                else if (fruit.shedsOnLanding()) { shedding.push_back(fruit.id()); fruit.landed(); Sound::emit(Sound::Sfx::FRUIT_DROP, fruit.body.position, landing); }
                else Sound::emit(Sound::Sfx::FRUIT_DROP, fruit.body.position, landing);
            }
        }
        for (const int id : cracking) {
            for (size_t i = 0; i < fruits.size(); ++i) {
                if (fruits[i].id() == id) { breakCoconut(fruits[i], false); break; }
            }
        }
        for (const int id : shedding) shedBananas(id);
        fruits.erase(std::remove_if(fruits.begin(), fruits.end(), [](const Fruit& f) { return f.finished(); }), fruits.end());
        if ((m_fruitTimer -= dt) > 0.0f) return;
        m_fruitTimer = RandomUtil::range(FRUIT_DROP_MIN, FRUIT_DROP_MAX);
        if (static_cast<int>(fruits.size()) >= MAX_FRUITS || player.body.underground) return;
        int onGround[3] = { 0, 0, 0 };
        for (const Fruit& fruit : fruits) if (fruit.available() && !fruit.loose()) ++onGround[static_cast<int>(fruit.kind())];
        std::vector<const ResourceNode*> trees[3]; // árboles cercanos por clase de fruto
        for (const ResourceNode& node : nodes) {
            Fruit::Kind treeKind;
            if (!fruitKind(node.typeId(), treeKind) || node.growing() || node.body.underground) continue;
            if (nearPlayer(node.body.position, FRUIT_RANGE)) trees[static_cast<int>(treeKind)].push_back(&node);
        }
        int pick = -1; // la clase con menos frutos en el suelo entre las que tienen árbol cerca
        for (int k = 0; k < 3; ++k) if (!trees[k].empty() && onGround[k] < MAX_PER_KIND && (pick < 0 || onGround[k] < onGround[pick])) pick = k;
        if (pick < 0) return;
        const ResourceNode& tree = *trees[pick][static_cast<size_t>(RandomUtil::integer(0, static_cast<int>(trees[pick].size()) - 1))];
        float half, height;
        ResourceStyle::hitbox(ResourceStyle::look(tree.typeId()), tree.growth(), half, height);
        const float angle = RandomUtil::range(0.0f, 6.2831853f), out = RandomUtil::range(0.8f, 1.8f);
        const DirectX::XMFLOAT3& at = tree.body.position;
        Fruit::Kind kind = Fruit::Kind::BANANA;
        fruitKind(tree.typeId(), kind);
        fruits.emplace_back(kind, ++m_fruitId, DirectX::XMFLOAT3{ at.x + std::sin(angle) * 0.5f, at.y + height * 0.9f, at.z + std::cos(angle) * 0.5f },
                            std::sin(angle) * out, std::cos(angle) * out);
        if (kind == Fruit::Kind::COCONUT && RandomUtil::roll(COCONUT_BREAK_CHANCE)) fruits.back().setBreaksOnLanding(true);
        if (kind == Fruit::Kind::BANANA) fruits.back().setShedsOnLanding(true);
    }

    // Los salvajes cercanos se acercan a comer un fruto caído o una baya madura de un arbusto y se quedan comiendo un momento.
    // El jugador tiene prioridad: si se acerca al objetivo, el pokémon lo deja.
    struct Forager { CaptureTarget* wild; int fruitId; int node; float time; };

    void updateForage(float dt) {
        constexpr float EAT_REACH = 0.9f, SEARCH = 14.0f, PLAYER_PRIORITY = 2.4f, GIVE_UP = 12.0f, EAT_TIME = 2.4f;
        // Los que ya van a por algo.
        for (size_t i = 0; i < m_foragers.size();) {
            Forager& f = m_foragers[i];
            bool keep = f.wild->shown() && f.wild->hittable() && f.wild != m_engaged && !f.wild->eating() && (f.time += dt) < GIVE_UP;
            DirectX::XMFLOAT3 goal{};
            Fruit* fruit = nullptr;
            ResourceNode* bush = nullptr;
            if (keep) {
                if (f.fruitId >= 0) {
                    for (Fruit& candidate : fruits) if (candidate.id() == f.fruitId && candidate.available()) fruit = &candidate;
                    keep = fruit != nullptr;
                    if (fruit) goal = fruit->body.position;
                } else {
                    bush = &nodes[f.node];
                    keep = bush->workable() && !bush->growing();
                    goal = bush->body.position;
                }
            }
            if (keep && nearPlayer(goal, PLAYER_PRIORITY)) keep = false;
            if (!keep) { if (f.wild->foraging() && !f.wild->eating()) f.wild->stopFight(); m_foragers.erase(m_foragers.begin() + static_cast<long>(i)); continue; }
            const float reach = EAT_REACH + (bush ? ResourceStyle::look(bush->typeId()).half : 0.0f);
            if (Geometry::within(goal, f.wild->body.position, reach)) {
                if (fruit) eatFruit(*fruit);
                else if (bush->work(false, 1.0f)) { bush->takeHarvest(); nodeSound(*bush); Sound::emit(Sound::Sfx::EAT, bush->body.position); }
                f.wild->eat(EAT_TIME);
                m_foragers.erase(m_foragers.begin() + static_cast<long>(i));
                continue;
            }
            f.wild->fightToward(goal, 0.2f);
            ++i;
        }
        // Nuevos: cada cierto tiempo, algunos salvajes deciden ir a por algo.
        if ((m_forageTimer -= dt) > 0.0f) return;
        m_forageTimer = 1.5f;
        if (player.body.underground) return;
        const auto claimedFruit = [&](int id) { for (const Forager& f : m_foragers) if (f.fruitId == id) return true; return false; };
        const auto claimedNode = [&](int index) { for (const Forager& f : m_foragers) if (f.fruitId < 0 && f.node == index) return true; return false; };
        for (CaptureTarget* wild : this->wild.loaded()) {
            if (!wild->shown() || !wild->hittable() || wild == m_engaged || wild->eating() || wild->foraging() || wild->body.underground) continue;
            if (!RandomUtil::roll(14.0f)) continue;
            float best = SEARCH * SEARCH;
            int fruitId = -1, node = -1;
            for (const Fruit& fruit : fruits) {
                if (!fruit.available() || !fruit.body.onGround || claimedFruit(fruit.id()) || nearPlayer(fruit.body.position, PLAYER_PRIORITY)) continue;
                const float d2 = Geometry::distance2XZ(fruit.body.position, wild->body.position);
                if (d2 < best) { best = d2; fruitId = fruit.id(); node = -1; }
            }
            if (fruitId < 0) {
                for (size_t n = 0; n < nodes.size(); ++n) {
                    const ResourceNode& bush = nodes[n];
                    if (ResourceStyle::look(bush.typeId()).shape != ResourceStyle::Shape::BUSH || bush.growing() || !bush.workable() || bush.body.underground) continue;
                    if (claimedNode(static_cast<int>(n)) || nearPlayer(bush.body.position, PLAYER_PRIORITY)) continue;
                    const float d2 = Geometry::distance2XZ(bush.body.position, wild->body.position);
                    if (d2 < best) { best = d2; node = static_cast<int>(n); }
                }
            }
            if (fruitId >= 0 || node >= 0) m_foragers.push_back({ wild, fruitId, node, 0.0f });
        }
    }

    // Huellas: botas del jugador, la forma de cada especie para su acompañante y los salvajes, y una marca redonda en lo que rueda.
    bool sandAt(float x, float z) {
        if (m_sandHabitats.empty()) {
            m_sandHabitats.assign(m_data->habitats.size(), 0);
            for (size_t i = 0; i < m_data->habitats.size(); ++i) m_sandHabitats[i] = (m_data->habitats[i].name == "Playa" || m_data->habitats[i].name == "Desierto") ? 1 : 0;
        }
        if (world.waterDepth(x, z) > 0.05f) return false;
        const int h = habitats.dominant(x, z);
        return h >= 0 && h < static_cast<int>(m_sandHabitats.size()) && m_sandHabitats[h];
    }

    void updateFootprints(float dt) {
        const auto sand = [&](float x, float z) { return sandAt(x, z); };
        if (!player.body.underground) {
            const DirectX::XMFLOAT3& p = player.body.position;
            footprints.step(&player, p.x, p.y + 0.01f, p.z, player.facing(), 1.0f, Footprints::Shape::BOOT, player.body.onGround && !player.body.underwater, sand);
            if (companion.active() && !companion.body.underground) {
                const DirectX::XMFLOAT3& c = companion.body.position;
                footprints.step(&companion, c.x, c.y + 0.01f, c.z, companion.yaw(), Companion::SIZE, Footprints::shapeOf(companion.speciesId()), companion.body.onGround && !companion.changing(), sand);
            }
            for (const CaptureTarget* creature : wild.loaded()) {
                if (!creature->shown() || creature->body.underground || creature->body.underwater) continue;
                const DirectX::XMFLOAT3& c = creature->body.position;
                if (!Geometry::within(c, p, FOOTPRINT_RANGE)) continue; // solo las cercanas se dibujan: no vale la pena marcar las lejanas
                footprints.step(creature, c.x, c.y + 0.01f, c.z, creature->yaw(), CaptureTarget::SIZE, Footprints::shapeOf(creature->speciesId()), creature->body.onGround, sand);
            }
            for (const Fruit& fruit : fruits) {
                if (std::hypot(fruit.body.velocity.x, fruit.body.velocity.z) < 0.4f) continue; // solo deja marca mientras rueda
                footprints.step(&fruit, fruit.body.position.x, fruit.body.position.y - Fruit::RADIUS + 0.01f, fruit.body.position.z, 0.0f, 0.2f, Footprints::Shape::ROUND, fruit.body.onGround, sand);
            }
            for (const Pokeball& ball : balls) {
                if (std::hypot(ball.body.velocity.x, ball.body.velocity.z) < 0.4f) continue;
                footprints.step(&ball, ball.body.position.x, ball.body.position.y - ball.body.groundOffset + 0.01f, ball.body.position.z, 0.0f, 0.2f, Footprints::Shape::ROUND, ball.body.onGround, sand);
            }
        }
        footprints.update(dt);
    }

    // Recoge un objeto suelto del mundo.
    void collect(GroundItem& item) {
        if (!item.available()) return;
        if (item.reward() >= 0) {
            giveEntry(m_data->rewards[item.reward()]);
            gainXp(Xp::PICK_UP);
        }
        item.take();
        Sound::emit(Sound::Sfx::PICK, item.body.position);
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

    // Se apunta al pokémon de otro entrenador: no se puede capturar, así que tampoco se lanza la pokéball contra él.
    bool aimingAtTrainerPokemon() const { return m_aimTarget && wild.foe() && m_aimTarget == wild.foe(); }

    // Pulsar lanzar: empieza la animación; la bola sale de la mano un instante después (throwBall).
    void startThrow() {
        throwCooldown = THROW_COOLDOWN;
        if (!inventory.canThrow()) {
            return;
        }
        m_throwDelay = THROW_DELAY;
        player.act(PlayerAction::THROW);
    }

    // Distancia a la que el rayo toca el relieve o la superficie del agua (maxDistance si no toca nada).
    float rayToSurface(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& dir, float maxDistance) const {
        const bool submerged = eye.y < world.waterLevel(eye.x, eye.z, eye.y) && world.waterDepth(eye.x, eye.z, eye.y) > 0.3f; // buceando se apunta al fondo, no a la superficie
        const auto below = [&](float t) {
            const float x = eye.x + dir.x * t, y = eye.y + dir.y * t, z = eye.z + dir.z * t;
            return y <= (submerged ? world.groundHeight(x, z, y) : world.surfaceHeight(x, z, y)) || world.solid(x, y, z);
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
    void launch(const DirectX::XMFLOAT3& aim, const DirectX::XMFLOAT3& dir, int ballIndex = -1) {
        if (ballIndex < 0) ballIndex = inventory.selectedIndex();
        const PokeballType& type = inventory.ball(ballIndex).type;
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
        inventory.consume(ballIndex);
        Sound::emit(Sound::Sfx::THROW, ball.body.position);
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
                noteBallContact(ball);
                ball.roll(h);
                ball.age += h;
                for (CaptureTarget* target : wild.loaded()) {
                    if (!target->hitBy(ball.body.position, Pokeball::RADIUS + Pokeball::CONTACT_MARGIN)) continue;
                    if (target == wild.foe()) { // el pokémon de un entrenador no se captura: la bola rebota en su cuerpo y se recupera al desaparecer
                        bounceOff(ball, *target);
                        break;
                    }
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

    // Rebote de una pokéball contra el cuerpo de un pokémon que no se puede capturar: se refleja la velocidad respecto a la normal (del centro del
    // pokémon a la bola) y se saca la bola de su hitbox para que no lo atraviese.
    void bounceOff(Pokeball& ball, const CaptureTarget& target) {
        const DirectX::XMFLOAT3 c = target.center();
        DirectX::XMFLOAT3& p = ball.body.position;
        DirectX::XMFLOAT3 n = { p.x - c.x, p.y - c.y, p.z - c.z };
        const float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (length < 0.01f) n = { -std::sin(camera.yaw()), 0.3f, -std::cos(camera.yaw()) }; // justo en el centro: rebota hacia el jugador
        else n = { n.x / length, n.y / length, n.z / length };
        DirectX::XMFLOAT3& v = ball.body.velocity;
        const float along = v.x * n.x + v.y * n.y + v.z * n.z;
        if (along < 0.0f) { // solo si se acerca (si ya se aleja, no se vuelve a reflejar)
            const float k = (1.0f + BALL_BOUNCE) * along;
            v = { v.x - k * n.x, v.y - k * n.y, v.z - k * n.z };
            Sound::emit(Sound::Sfx::BALL_HIT, p);
        }
        for (int i = 0; i < 16 && target.hitBy(p, Pokeball::RADIUS + Pokeball::CONTACT_MARGIN); ++i) p = { p.x + n.x * 0.1f, p.y + n.y * 0.1f, p.z + n.z * 0.1f };
        if (!ball.bounced) { ball.bounced = true; showNotice(Notice::REWARD, "¡No puedes capturar a los pokémon de otros entrenadores!"); }
    }

    // Suena al tocar el suelo o caer al agua (una sola vez por contacto).
    void noteBallContact(Pokeball& ball) {
        const DirectX::XMFLOAT3& p = ball.body.position;
        const bool wet = p.y - ball.body.groundOffset < world.waterLevel(p.x, p.z, p.y) && world.waterDepth(p.x, p.z, p.y) > 0.1f;
        if (wet && !ball.wet) Sound::emit(Sound::Sfx::BALL_SPLASH, p);
        else if (!wet && ball.body.onGround && !ball.wasGround) Sound::emit(Sound::Sfx::BALL_HIT, p);
        ball.wet = wet;
        ball.wasGround = ball.body.onGround;
    }

    // Una bola que no tocó a ningún pokémon vuelve al inventario al desaparecer (la que alcanzó a uno se gasta).
    void returnBall(const Pokeball& ball) {
        if (ball.spent) return;
        const Item* item = m_data->item(ball.typeId);
        if (!item || !inventory.add(item->id, 1)) return;
        if (noticeTime <= 0.0f) showNotice(Notice::REWARD, "1x " + m_data->itemName(*item));
    }

    struct Interaction {
        enum Kind { NONE, CHEST, NODE, GROUND, MACHINE, PROFESSOR, WEAKENED, FRUIT, PLOT, TRAINER } kind = NONE;
        int index = -1;
    };

    // La interacción vigente con su índice comprobado: la lista a la que apunta pudo cambiar de tamaño (un objeto recogido o borrado) desde que se calculó.
    // Si ya no es válida se descarta y queda en el registro (un índice viejo leyendo fuera de la lista sería un cierre del juego).
    Interaction validInteraction() const {
        const Interaction i = m_interaction;
        size_t count = static_cast<size_t>(-1);
        switch (i.kind) {
            case Interaction::CHEST:   count = chests.entities.size(); break;
            case Interaction::NODE:    count = nodes.size(); break;
            case Interaction::GROUND:  count = groundItems.entities.size(); break;
            case Interaction::FRUIT:   count = fruits.size(); break;
            case Interaction::TRAINER: count = trainers.size(); break;
            case Interaction::PLOT:    count = plots.size(); break;
            default: break;
        }
        if (count == static_cast<size_t>(-1) || (i.index >= 0 && static_cast<size_t>(i.index) < count)) return i;
        static int reported = 0;
        if (reported++ < 20) Logger::logInfo("WARN_INTERACTION", "Índice de interacción fuera de rango: tipo=" + std::to_string(static_cast<int>(i.kind)) + " índice=" + std::to_string(i.index) + " tamaño=" + std::to_string(count));
        return {};
    }

    // Hay algún pokémon al alcance al que fijar la cámara (o ya hay uno fijado).
    bool anyLockable() const {
        return lockedTarget || !lockCandidates(nullptr).empty();
    }

    static constexpr const char* CHEST_VERB = "Abrir";
    static constexpr const char* PLANT_VERB = "Plantar";
    static constexpr const char* CAPTURE_VERB = "Capturar";
    static constexpr float FOOTPRINT_RANGE = 70.0f; // solo las criaturas cercanas dejan huellas
    static constexpr float RADIO_FADE_IN = 0.18f, RADIO_FADE_OUT = 0.35f, RADIO_PICK_HOLD = 0.25f; // la rueda aparece rápido y se va despacio
    static constexpr const char* PICK_VERB = "Recoger";
    static constexpr const char* USE_VERB = "Usar";
    static constexpr const char* TALK_VERB = "Hablar con";
    static constexpr const char* FIGHT_VERB = "Retar a";
    static constexpr const char* SHOP_VERB = "Comprar a";
    inline static const std::string PROFESSOR_NAME = Professor::NAME;
    inline static const std::string PLOT_NAME = "Parcela de cultivo";
    inline static const std::string GROUND_NAME = "objeto brillante";
    inline static const std::string MACHINE_NAME = "máquina de investigación";
    static constexpr float CAVE_TEMPERATURE = 12.0f;   // °C dentro de la cueva, la mina y la isla del cielo
    static constexpr float MINE_TEMPERATURE = 14.0f;
    static constexpr float SKY_TEMPERATURE = 8.0f;

    const GameData* m_data;
    static constexpr float FLOWER_CHECK_SECONDS = 2.0f; // cada cuánto se intenta hacer brotar una flor
    float m_flowerTimer = 0.0f;
    std::vector<float> m_flowerWeights;
    int m_seedCrop = -1;       // cultivo cuyas semillas se siembran
    int m_pendingPlot = -1;    // parcela a la que se siembra cuando se elige la semilla
    float m_groundTimer = 0.0f; // reloj de aparición de los objetos sueltos del mundo
    float m_clock = 0.0f;       // segundos de partida (marea)
    static constexpr float RESPAWN_TIME = 3.4f; // duración de la pantalla de carga tras desmayarse (s)
    float m_respawn = 0.0f;
    std::string m_deathTitle, m_deathDetail; // lo que cuenta la pantalla de carga tras desmayarse
    static constexpr const char* RESPAWN_PLACE = "el punto de inicio"; // pasará a ser el último centro pokémon visitado
    bool m_respawnMoved = false;
    float m_swimLatch = 0.0f;
    float m_swimLogTime = 0.0f;
    mutable float m_viewPull = 1.0f, m_viewEyeY = 0.0f;
    float m_cascadeTime = 0.0f; // segundos que lleva el jugador en una cascada con un pokémon que la remonta
    static constexpr float CASCADE_RANGE = 5.0f;     // a esta distancia de la cascada está en ella
    static constexpr float CASCADE_SEEK = 30.0f;     // si pasan por una a menos de esto, va a intentar subirla
    static constexpr float CASCADE_SUCCESS = 0.2f;   // probabilidad de que un intento lo logre
    static constexpr float CASCADE_REST_MIN = 150.0f, CASCADE_REST_MAX = 260.0f; // segundos entre un intento y el siguiente
    static constexpr float CASCADE_TOP_REACH = 16.0f; // hasta dónde busca la cima de la cascada
    DirectX::XMFLOAT3 m_cascadeGoal = { 0.0f, 0.0f, 0.0f };
    float m_cascadeRest = 0.0f;
    static constexpr float CASCADE_TIME = 3.0f;          // segundos que el jugador pasa en la cascada con el pokémon al lado para que evolucione
    static constexpr float CASCADE_PLAYER_RANGE = 3.5f; // a esta distancia de una cascada el jugador está en ella
    static constexpr float CASCADE_TOGETHER = 9.0f;     // el pokémon debe estar así de cerca del jugador
    static constexpr float THROW_BUFFER = 0.35f;      // segundos que se recuerda un clic de lanzar que no pudo salir al instante
    static constexpr float BALL_BOUNCE = 0.55f; // fracción de la velocidad que conserva la bola al rebotar en un pokémon
    float m_throwQueued = 0.0f;
    float m_radioHold = 0.0f; // al elegir emisora, lo que le queda a la rueda antes de irse
    float m_throwDelay = -1.0f; // >= 0: lanzando (la bola sale al llegar a 0)
    int m_fishSkill = -1;       // habilidad de pesca (la de la animación FISH)
    bool m_castHint = false;    // se puede lanzar la caña aquí
    static constexpr float SEEN_RANGE = 30.0f;         // a esta distancia (y a la vista) un pokémon cuenta como visto en la Pokédex
    static constexpr float COLLECT_WINDOW = 8.0f;      // segundos seguidos que recolecta cada vez
    static constexpr float COLLECT_WAIT_MIN = 30.0f;   // segundos que pasa siguiendo al jugador entre una recolección y otra
    static constexpr float COLLECT_WAIT_MAX = 60.0f;
    static constexpr float HOLD_REPEAT = 0.45f;        // segundos entre golpes al mantener pulsado interactuar
    float m_collectClock = 12.0f;
    static constexpr float BALL_SOLID = 0.26f; // semiancho de la caja de la pokéball mientras captura
    enum class DuelTurn { COMPANION, WILD };
    static constexpr float WILD_TURN_DELAY = 0.9f;     // pausa antes de que responda el salvaje
    static constexpr float WILD_TURN_TIMEOUT = 4.0f;   // si no llega a su rival en este tiempo, pasa el turno
    static constexpr float WILD_DAMAGE_SCALE = 0.8f;   // los salvajes pegan algo menos que el compañero
    DuelTurn m_duelTurn = DuelTurn::COMPANION;
    float m_turnClock = 0.0f;
    float m_leftoversTimer = 0.0f;
    static constexpr float CRITICAL_MULTIPLIER = 1.5f, LEFTOVERS_INTERVAL = 6.0f;
    static constexpr int LIFE_ORB_RECOIL_DIVISOR = 20; // la Vidasfera quita 1/20 de los PS máximos en cada golpe
    float m_healAcc = 0.0f;                            // PS fraccionarios acumulados al curarse poco a poco

    public:
    // --- Entrenadores ---
    // Pokémon rival de prueba: una especie al azar del nivel de nuestro líder (o cercano).
    OwnedPokemon rivalPokemon(int level) const {
        OwnedPokemon rival;
        const OwnedPokemon* lead = storage.active();
        const PokemonSpecies* mine = lead ? m_data->speciesById(lead->speciesId) : nullptr;
        const PokemonSpecies* species = nullptr;
        for (int tries = 0; tries < 30 && (!species || (mine && !DamageRules::compute(*m_data, *mine, level, *species, level).lands())); ++tries) { // nunca uno al que el líder no pueda hacer daño
            species = &m_data->species[static_cast<size_t>(RandomUtil::integer(0, static_cast<int>(m_data->species.size()) - 1))];
        }
        rival.speciesId = species->id;
        rival.level = (std::max)(2, level);
        storage.ensureMoves(rival);
        return rival;
    }
    int leadLevel() const { const OwnedPokemon* lead = storage.active(); return lead ? lead->level : 5; }

    static BattleRules::OwnedLike likeOf(const OwnedPokemon& o, int source) { return { o.speciesId, o.level, o.shiny, o.hp, o.heldItemId, o.moves, o.pp, o.ivs, o.evs, o.nature, source }; }

    // Empieza el duelo con un entrenador: en tiempo real (su pokémon sale al mundo y peleamos como contra los salvajes) o por turnos (pantalla propia).
    void startDuel(int index) {
        CRASH_SCOPE();
        Trainer& trainer = trainers[index];
        if (battle || m_duelTrainer >= 0 || trainer.busy) return;
        if (storage.team().empty() || storage.nextUsable() < 0) { showNotice(Notice::REWARD, "Tus pokémon no están en condiciones de combatir"); return; }
        trainer.busy = true;
        if (trainer.style == Trainer::Style::REAL_TIME) {
            m_duelTrainer = index;
            m_duelLeft = DUEL_POKEMON;
            showNotice(Notice::REWARD, trainer.name + " quiere combatir");
            sendDuelPokemon();
            return;
        }
        std::vector<BattleRules::Fighter> mine, foes;
        for (int i = 0; i < static_cast<int>(storage.team().size()); ++i) mine.push_back(BattleRules::makeFighter(*m_data, likeOf(storage.team()[i], i)));
        const bool doubles = trainer.style == Trainer::Style::DOUBLES;
        for (int i = 0; i < DUEL_POKEMON; ++i) foes.push_back(BattleRules::makeFighter(*m_data, likeOf(rivalPokemon(leadLevel()), -1)));
        int first = -1; // siempre sale el primero del equipo (que no esté debilitado), sea cual sea el que nos siga en el mundo
        for (int i = 0; i < static_cast<int>(storage.team().size()) && first < 0; ++i) if (!storage.fainted(storage.team()[static_cast<size_t>(i)])) first = i;
        battle = std::make_shared<Battle>(*m_data, std::move(mine), first, std::move(foes), trainer.name, doubles);
        m_battleTrainer = index;
        Logger::logInfo("BATTLE", "Empieza el combate por turnos contra " + trainer.name + " | dobles=" + std::to_string(doubles) + " | equipo propio=" + std::to_string(storage.team().size()) + " | rivales=" + std::to_string(DUEL_POKEMON) + " | nivel del líder=" + std::to_string(leadLevel()));
        m_battleOrigin = player.body.position;     // a donde vuelve el jugador al terminar
        m_trainerHome = trainer.body.position;     // y el entrenador
        m_battleOrbit = BATTLE_START_ORBIT;        // la cámara empieza detrás de nuestro pokémon, algo girada: el nuestro a la izquierda y el rival a la derecha
        m_battleTilt = 0.35f;
        m_battleSlide = 0.0f;                      // durante la carga ambos se desplazan a los bordes de la zona de combate
        m_battleYaw = std::atan2(arenaDirection().x, arenaDirection().z);
        battleFx = {};
        battleRequested = true;
        player.body.velocity = { 0.0f, 0.0f, 0.0f };
    }

    // Saca el siguiente pokémon del entrenador de tiempo real junto a él.
    void sendDuelPokemon() {
        const Trainer& trainer = trainers[m_duelTrainer];
        const OwnedPokemon rival = rivalPokemon(leadLevel());
        const int index = m_data->speciesIndex(rival.speciesId);
        const DirectX::XMFLOAT3 toPlayer = { player.body.position.x - trainer.body.position.x, 0.0f, player.body.position.z - trainer.body.position.z };
        const float length = (std::max)(0.01f, std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z));
        const float x = trainer.body.position.x + toPlayer.x / length * 3.0f, z = trainer.body.position.z + toPlayer.z / length * 3.0f;
        wild.spawnFoe(x, (std::max)(trainer.body.position.y, world.surfaceHeight(x, z)), z, std::atan2(toPlayer.x, toPlayer.z), index, rival.speciesId, rival.level);
        m_engaged = nullptr;
        // El entrenador lanza su pokéball (la misma animación que el acompañante: vuela, se abre, el rayo saca al pokémon y vuelve).
        m_trainerBall.start(BallTransfer::Mode::RELEASE, std::atan2(toPlayer.x, toPlayer.z), { trainer.body.position.x, trainer.body.position.y + 1.3f, trainer.body.position.z }, RIVAL_BALL);
    }

    // Duelo en tiempo real: cuando cae su pokémon saca otro; sin más, gana el jugador. Si el jugador cae, se termina.
    // Correa del duelo en tiempo real: ningún pokémon se aleja de su entrenador más de DUEL_LEASH (el rival, del suyo; el nuestro, de nosotros).
    static void leash(Physics::Body& body, const DirectX::XMFLOAT3& owner) {
        const float dx = body.position.x - owner.x, dz = body.position.z - owner.z, d2 = dx * dx + dz * dz;
        if (d2 <= DUEL_LEASH * DUEL_LEASH) return;
        const float k = DUEL_LEASH / std::sqrt(d2);
        body.position.x = owner.x + dx * k;
        body.position.z = owner.z + dz * k;
    }
    void updateTrainerDuel(float dt) {
        CRASH_SCOPE();
        if (m_trainerBall.active()) {
            const CaptureTarget* foe = wild.foe();
            const DirectX::XMFLOAT3 hover = foe ? foe->center() : m_trainerBall.pose().position;
            const DirectX::XMFLOAT3 hand = m_duelTrainer >= 0 ? DirectX::XMFLOAT3{ trainers[m_duelTrainer].body.position.x, trainers[m_duelTrainer].body.position.y + 1.3f, trainers[m_duelTrainer].body.position.z } : hover;
            m_trainerBall.update(dt, hover, hand);
        }
        if (m_duelTrainer < 0) return;
        if (CaptureTarget* foe = wild.foe()) leash(foe->body, trainers[m_duelTrainer].body.position);
        if (storage.active()) leash(companion.body, player.body.position);
        if (m_respawn > 0.0f || storage.teamWiped()) { endDuel(false); return; }
        if (!wild.foeActive()) { // su pokémon ha caído
            if (!m_duelNext) {
                if (--m_duelLeft <= 0) { endDuel(true); return; }
                m_duelNext = true;
                m_duelPause = DUEL_PAUSE;
                return;
            }
            if ((m_duelPause -= dt) <= 0.0f) { m_duelNext = false; sendDuelPokemon(); }
        }
    }
    void endDuel(bool won) {
        if (m_duelTrainer < 0) return;
        Trainer& trainer = trainers[m_duelTrainer];
        trainer.busy = false;
        m_duelTrainer = -1;
        m_duelNext = false;
        wild.dismissFoe();
        m_engaged = nullptr;
        if (won) showNotice(Notice::REWARD, "¡Has derrotado a " + trainer.name + "!");
    }

    // Termina el combate por turnos: PS y PP vuelven a los pokémon del jugador y los que combatieron ganan experiencia si ganó.
    void finishBattle() {
        CRASH_SCOPE();
        if (!battle) return;
        Logger::logInfo("BATTLE", std::string("Termina el combate: ") + (battle->outcome() == Battle::Outcome::WON ? "victoria" : battle->outcome() == Battle::Outcome::LOST ? "derrota" : "sin resultado") + " | rivales derrotados=" + std::to_string(battle->defeatedLevels().size()));
        for (const BattleRules::Fighter& f : battle->playerTeam()) {
            OwnedPokemon* owned = storage.mutableAt(true, f.source);
            if (!owned) continue;
            owned->hp = f.hp;
            owned->pp = f.pp;
        }
        // Sin dinero ni premios: cada pokémon cobra lo que ganó al derrotar rivales (ya repartido entre los que seguían en pie) y el jugador, la suya.
        std::string text;
        for (const BattleRules::Fighter& f : battle->playerTeam()) {
            if (f.xpGain <= 0 || storage.team()[static_cast<size_t>(f.source)].level >= progress.levelCap()) continue; // al nivel máximo no gana experiencia
            const PokemonStorage::XpResult result = storage.addXp(f.source, f.xpGain, progress.levelCap());
            text += (text.empty() ? "" : "  ") + f.name + " +" + std::to_string(f.xpGain) + " PX";
            if (result.levels > 0) text += " (Nv. " + std::to_string(storage.team()[static_cast<size_t>(f.source)].level) + ")";
        }
        int playerGain = 0;
        for (const int foeLevel : battle->defeatedLevels()) playerGain += ExperienceRules::playerXp(foeLevel, progress.level());
        if (playerGain > 0) {
            text += (text.empty() ? "" : "  ") + std::string("Tú +") + std::to_string(playerGain) + " PX";
            if (progress.addXp(playerGain) > 0) text += " (Nv. " + std::to_string(progress.level()) + ")";
        }
        if (m_battleTrainer >= 0 && battle->outcome() == Battle::Outcome::WON && trainers[m_battleTrainer].gym && !trainers[m_battleTrainer].badgeGiven) {
            trainers[m_battleTrainer].badgeGiven = true; // medalla de prueba: una sola vez por sesión
            progress.addBadge();
            Logger::logInfo("BATTLE", "Medalla de gimnasio de " + trainers[m_battleTrainer].name + " | medallas=" + std::to_string(progress.badges()) + " | nivel máximo=" + std::to_string(progress.levelCap()));
            text += (text.empty() ? "" : "  ") + std::string("¡Medalla de gimnasio! Nivel máximo ") + std::to_string(progress.levelCap());
        }
        if (!text.empty()) showNotice(Notice::REWARD, text);
        if (m_battleTrainer >= 0) {
            trainers[m_battleTrainer].busy = false;
            trainers[m_battleTrainer].body.position = m_trainerHome;
        }
        player.body.position = m_battleOrigin;
        player.body.velocity = { 0.0f, 0.0f, 0.0f };
        m_battleTrainer = -1;
        battle.reset();
    }

    // Movimientos nuevos: los aprendidos solos se avisan; los que no caben abren la pantalla de decisión (nunca durante un combate).
    void updateLearning() {
        if (battle) return;
        for (const auto& entry : storage.m_learnedNow) {
            const PokemonSpecies* species = m_data->speciesById(entry.first);
            const Move* move = m_data->move(entry.second);
            if (species && move) showNotice(Notice::REWARD, species->name + " aprendió " + move->name);
        }
        storage.m_learnedNow.clear();
        if (!learnRequested && !battleRequested && storage.pendingLearnRequest().index >= 0) learnRequested = true;
    }

    // --- Tienda: vende todo lo relacionado con los pokémon (apartado "Pokémon" de la mochila) que tenga precio de compra. Un precio de compra 0 lo
    // deja fuera (Honor Ball, Shiny Ball...): se decide en la base de datos, no aquí. ---
    std::vector<const Item*> shopStock() const {
        std::vector<const Item*> stock;
        for (const Item& item : m_data->items) {
            if (item.buyPrice <= 0) continue;
            for (const ItemCategoryInfo& category : m_data->categories) {
                if (category.id == item.categoryId && category.section == "Pokémon") { stock.push_back(&item); break; }
            }
        }
        return stock;
    }
    // Compra 'quantity' unidades (o las que alcance el dinero). Devuelve el texto del resultado ("" si no pudo comprar nada).
    std::string shopBuy(int itemId, int quantity) {
        const Item* item = m_data->item(itemId);
        if (!item || item->buyPrice <= 0 || quantity <= 0) return {};
        const int units = (std::min)(quantity, inventory.money() / item->buyPrice);
        if (units <= 0) return {};
        inventory.add(itemId, units);
        inventory.addMoney(-units * item->buyPrice);
        std::string text = std::to_string(units) + "x " + m_data->itemName(*item) + " comprado";
        const ShopRules::Gift gift = ShopRules::giftFor(*m_data, *item, units, m_shopBought[itemId]);
        if (gift.reward && gift.count > 0) {
            for (int i = 0; i < gift.count; ++i) giveEntry(*gift.reward);
            text += " (¡y de regalo!)";
        }
        return text;
    }

    // --- Zona de combate por turnos: un círculo en el suelo junto al inicio. El entrenador está en un punto del borde (por fuera) y el jugador
    // en el opuesto, uno frente al otro; los pokémon pelean en medio. ---
    static constexpr float ARENA_RADIUS = 10.0f;      // radio de la zona (m)
    static constexpr float DOUBLES_SPREAD = 2.0f;     // en dobles, cada pokémon de un lado se separa así del eje central
    static constexpr float BATTLE_START_ORBIT = 0.95f; // giro inicial de la cámara (rad) desde la vista lateral hacia la espalda de nuestro pokémon
    static constexpr float ARENA_EDGE_GAP = 0.9f;     // el jugador y el entrenador rival están así de lejos del borde, por fuera del círculo y en lados opuestos
    static constexpr float ARENA_POKEMON = 4.0f;      // cada pokémon, así de lejos del centro
    DirectX::XMFLOAT3 arenaCenter() const {
        const float x = ARENA_X, z = ARENA_Z;
        return { x, (std::max)(Physics::World::START_HEIGHT, world.surfaceHeight(x, z)), z };
    }
    // Del jugador hacia el entrenador (unitario, en el plano).
    static DirectX::XMFLOAT3 arenaDirection() { return { 1.0f, 0.0f, 0.0f }; }
    // Dónde está cada entrenador (lado 0 = jugador, 1 = rival) cuando ya han tomado sitio.
    DirectX::XMFLOAT3 arenaStand(int side) const {
        const DirectX::XMFLOAT3 c = arenaCenter(), d = arenaDirection();
        const float r = ARENA_RADIUS + ARENA_EDGE_GAP, k = side == 1 ? r : -r;
        const float x = c.x + d.x * k, z = c.z + d.z * k;
        return { x, (std::max)(c.y, world.surfaceHeight(x, z)), z };
    }

    // Vista del combate por turnos: detrás de nuestro pokémon, mirando hacia el rival.
    // 'slot' = hueco del campo (Battle::slotOf): lado * 2 + posición. En dobles los dos de un lado se reparten a izquierda y derecha.
    DirectX::XMFLOAT3 battleSpot(int slot) const {
        const DirectX::XMFLOAT3 c = arenaCenter(), d = arenaDirection();
        const int side = Battle::sideOf(slot);
        const float k = side == 1 ? ARENA_POKEMON : -ARENA_POKEMON;
        const float lateral = battle && battle->doubles() ? (Battle::posOf(slot) == 0 ? -DOUBLES_SPREAD : DOUBLES_SPREAD) : 0.0f;
        return { c.x + d.x * k - d.z * lateral, c.y, c.z + d.z * k + d.x * lateral };
    }
    // Tiempo y desplazamiento de la embestida del que ataca y el temblor del que recibe (en metros hacia delante y a un lado).
    DirectX::XMFLOAT3 battleOffset(int slot) const {
        const float t = battleFx.time;
        const float dx = std::sin(m_battleYaw), dz = std::cos(m_battleYaw);
        float forward = 0.0f, lateral = 0.0f;
        if (battleFx.attacker == slot) forward = (Battle::sideOf(slot) == 0 ? 1.0f : -1.0f) * 1.4f * std::sin((std::min)(t / 0.35f, 1.0f) * 3.14159265f);
        else if (battleFx.attacker >= 0 && battleFx.hit && battleFx.target == slot && t > 0.25f && t < 0.75f) lateral = 0.18f * std::sin((t - 0.25f) * 60.0f);
        return { dx * forward + dz * lateral, 0.0f, dz * forward - dx * lateral };
    }
    // Cámara del combate por turnos: de lado a la línea de los pokémon (el nuestro a la izquierda, el rival a la derecha) y se puede girar
    // alrededor de la zona de combate (battleOrbit) y subir o bajar.
    void battleOrbit(float yawDelta, float tiltDelta) {
        m_battleOrbit += yawDelta;
        m_battleTilt = (std::clamp)(m_battleTilt + tiltDelta, 0.05f, 1.0f);
    }
    DirectX::XMMATRIX battleView() const {
        const DirectX::XMFLOAT3 c = arenaCenter(), d = arenaDirection();
        const float yaw = std::atan2(-d.z, d.x) + m_battleOrbit; // mirando perpendicular a la línea nuestro→rival (nuestro pokémon a la izquierda)
        const float dx = std::sin(yaw), dz = std::cos(yaw);
        const float dist = ARENA_RADIUS - 0.5f, height = 1.6f + 6.5f * m_battleTilt; // siempre desde el borde del círculo; la inclinación solo la sube o baja
        const DirectX::XMFLOAT3 eye = { c.x - dx * dist, c.y + height, c.z - dz * dist };
        const DirectX::XMFLOAT3 at = { c.x, c.y + 0.7f, c.z };
        return DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&at), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    }
    // En dobles: ¿el pokémon del hueco 0 de cada lado se ve a la izquierda de la pantalla? (depende de cómo se haya girado la cámara)
    bool battleFirstOnLeft() const {
        const DirectX::XMFLOAT3 d = arenaDirection();
        const float yaw = std::atan2(-d.z, d.x) + m_battleOrbit;
        return (-d.z * std::cos(yaw) - d.x * std::sin(yaw)) >= -0.01f;
    }
    float battleYaw() const { return m_battleYaw; }
    int battleTrainer() const { return m_battleTrainer; }
    DirectX::XMFLOAT3 battleHand(int side) const { const DirectX::XMFLOAT3 stand = arenaStand(side); return { stand.x, stand.y + 1.3f, stand.z }; }
    void advanceBattleFx(float dt) {
        CRASH_SCOPE();
        battleFx.time += dt;
        if (battle) for (int slot = 0; slot < Battle::SLOTS; ++slot) {
            if (!m_battleBalls[slot].active()) continue;
            const DirectX::XMFLOAT3 spot = battleSpot(slot);
            m_battleBalls[slot].update(dt, { spot.x, spot.y + 1.6f, spot.z }, battleHand(Battle::sideOf(slot)));
        }
        if (battle) player.face(std::atan2(arenaStand(1).x - arenaStand(0).x, arenaStand(1).z - arenaStand(0).z)); // el jugador mira al rival
        if (!battle || m_battleSlide >= 1.0f) return;
        // Durante la pantalla de carga el entrenador y el jugador caminan hasta sus sitios del borde (con suavizado).
        m_battleSlide = (std::min)(1.0f, m_battleSlide + dt / BATTLE_SLIDE_TIME);
        const float e = m_battleSlide * m_battleSlide * (3.0f - 2.0f * m_battleSlide);
        const DirectX::XMFLOAT3 mine = arenaStand(0), theirs = arenaStand(1);
        player.body.position = { m_battleOrigin.x + (mine.x - m_battleOrigin.x) * e, m_battleOrigin.y + (mine.y - m_battleOrigin.y) * e, m_battleOrigin.z + (mine.z - m_battleOrigin.z) * e };
        if (m_battleTrainer >= 0) trainers[m_battleTrainer].body.position = { m_trainerHome.x + (theirs.x - m_trainerHome.x) * e, m_trainerHome.y + (theirs.y - m_trainerHome.y) * e, m_trainerHome.z + (theirs.z - m_trainerHome.z) * e };
    }
    // Empieza la animación de un mensaje del combate (quién ataca y si llega el golpe).
    void playBattleFx(const Battle::Message& m) {
        CRASH_SCOPE();
        battleFx = { m.attacker, m.target, m.hit, 0.0f };
        if (!battle || (m.animKind != 1 && m.animKind != 2) || m.animSide < 0) return;
        const int slot = m.animSide, side = Battle::sideOf(slot);
        const DirectX::XMFLOAT3 hand = battleHand(side);
        int ball = RIVAL_BALL; // los del jugador, con la pokéball de cada uno; los del rival, la Honor Ball
        if (side == 0 && battle->occupied(slot)) if (const OwnedPokemon* owned = storage.mutableAt(true, battle->fighter(slot).source)) ball = owned->ballId;
        const bool release = m.animKind == 2;
        m_battleBalls[slot].start(release ? BallTransfer::Mode::RELEASE : BallTransfer::Mode::ABSORB, m_battleYaw + (side == 0 ? 0.0f : 3.14159265f), hand, ball);
        Sound::emit(release ? Sound::Sfx::BALL_RELEASE : Sound::Sfx::BALL_RECALL, hand);
    }
    static constexpr int RIVAL_BALL = 6; // pokéball de los pokémon de los entrenadores (id de pokeball.hpp)
    const BallTransfer* trainerBall() const { return m_trainerBall.active() ? &m_trainerBall : nullptr; }
    const BallTransfer* battleBall(int slot) const { return m_battleBalls[slot].active() ? &m_battleBalls[slot] : nullptr; }

    private:

    static constexpr int DUEL_POKEMON = 2;
    static constexpr float DUEL_PAUSE = 0.35f;
    static constexpr float BATTLE_SLIDE_TIME = 1.8f; // lo que tardan en colocarse (menos que la pantalla de carga)
    static constexpr float ARENA_X = -3.0f, ARENA_Z = 17.5f; // centro de la zona: al sur de los entrenadores
    DirectX::XMFLOAT3 m_trainerHome = {};
    std::map<int, int> m_shopBought; // unidades compradas de cada objeto que aún no han dado regalo
    float m_battleSlide = 1.0f;
    BallTransfer m_battleBalls[Battle::SLOTS]; // pokéballs de los pokémon que salen o vuelven en el combate por turnos
    float m_battleOrbit = 0.0f, m_battleTilt = 0.35f; // giro y altura de la cámara del combate
    static constexpr float DUEL_LEASH = 9.0f; // metros máximos entre un pokémon del duelo en tiempo real y su entrenador
    BallTransfer m_trainerBall; // pokéball del entrenador al sacar a su pokémon en un duelo en tiempo real
    int m_duelTrainer = -1;
    int m_duelLeft = 0;
    float m_duelPause = 0.0f;
    bool m_duelNext = false;
    int m_battleTrainer = -1;
    DirectX::XMFLOAT3 m_battleOrigin = {};
    float m_battleYaw = 0.0f;
    CaptureTarget* m_engaged = nullptr; // salvaje al que el acompañante está atacando ahora
    float m_captureRest = 0.0f; // descanso del acompañante entre ataques en modo captura
    bool m_collecting = false;
    int m_taskNode = -1; // índice del nodo que el acompañante ha empezado a extraer (-1 = ninguno)
    float m_holdClock = 0.0f;
    struct PendingPotion { const TrainingItem* type = nullptr; int uid = 0; float wait = 0.0f; } m_pendingPotion; // poción o revivir a la espera de que el pokémon salga
    int m_itemEvolveUid = 0;   // pokémon que evoluciona por un objeto y aún no ha hecho el efecto (0 = ninguno)
    int m_shownUid = 0;        // pokémon (uid y especie) que está fuera o saliendo
    int m_shownSpecies = -1;
    int m_missingSkill = -1;   // habilidad que falta para el recurso cercano (-1 = ninguna)
    DirectX::XMFLOAT3 m_missingPos = {};
    int m_missingLevel = 0;    // nivel que exige ese recurso
    Interaction m_interaction; // lo que el jugador puede usar ahora mismo (cofre o recurso cercano)
    DirectX::XMFLOAT3 m_home = {};         // dónde reaparece el jugador al quedarse sin vida
    int m_pool = -1;         // charca termal en la que está (-1 = ninguna)
    float m_poolTime = 0.0f;   // segundos en esa charca
    bool m_concealed = false;              // el jugador está oculto entre matorrales: los pokémon no lo han detectado (sigilo)
    const CaptureTarget* m_weakSeen = nullptr;
    float m_weakAge = 0.0f;
    bool m_weakExpired = false;
    float m_fruitTimer = 6.0f;
    int m_fruitId = 0;
    float m_forageTimer = 2.0f;
    std::vector<Forager> m_foragers;
    std::vector<char> m_sandHabitats;
    const CaptureTarget* m_weak = nullptr; // salvaje debilitado por el acompañante que se puede capturar ahora (Interaction::WEAKENED)
    bool m_aiming = false; // modo lanzamiento (clic derecho) o L2 mantenido
    bool m_wasAiming = false; // el fotograma anterior estaba en modo captura (para elegir la primera pokéball al entrar)
    float m_lockHidden = 0.0f; // segundos que lleva tapado el pokémon fijado
    CaptureTarget* m_aimTarget = nullptr;  // pokémon al que apunta la cruceta dentro del alcance
};
