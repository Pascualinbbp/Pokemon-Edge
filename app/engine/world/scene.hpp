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
#include "spawn/chestSpawn.hpp"
#include "rules/captureRules.hpp"
#include "rules/companionRules.hpp"
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
#include "state/inventory.hpp"
#include "entities/player.hpp"
#include "entities/pokeball.hpp"
#include "state/pokemonStorage.hpp"
#include "spawn/resourceSpawn.hpp"
#include "spawn/spawnField.hpp"

struct Scene {
    static constexpr float HALF_SIZE = Physics::World::HALF_SIZE;
    static constexpr int TARGET_COUNT = 4;

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
    static constexpr size_t MAX_BALLS = 12;
    static constexpr float MAX_SUBSTEP = 1.0f / 120.0f; // paso máximo de la física de los proyectiles
    static constexpr float NOTICE_TIME = 2.0f;
    static constexpr const char* CLEAR_SKY = "Despejado";

    Physics::World world;
    Player player;
    Camera camera;
    std::array<CaptureTarget, TARGET_COUNT> targets = { CaptureTarget(0), CaptureTarget(1), CaptureTarget(2), CaptureTarget(3) };
    std::vector<Pokeball> balls;
    DayCycle dayCycle;
    HabitatMap habitats;      // zonas de cada hábitat, generadas al empezar la partida
    WeatherSystem weather;    // clima de cada hábitat
    Inventory inventory;
    SpawnField<Chest> chests;
    SpawnField<ResourceNode> nodes;
    SpawnField<GroundItem> groundItems;
    PokemonStorage storage;   // equipo (6) y PC
    PlayerProgress progress;  // nivel, experiencia y medallas del jugador
    ResearchMachine machine;  // máquina de investigación (fija)
    bool researchRequested = false; // el jugador ha usado la máquina: la interfaz abre su pantalla
    Companion companion;      // el líder del equipo, que acompaña al jugador y trabaja recursos
    std::string noticeText;   // segunda línea del aviso: recompensa obtenida, pokémon capturado...

    Notice notice = Notice::NONE;
    float noticeTime = 0.0f;
    float throwCooldown = 0.0f;
    bool aimMode = false;     // modo lanzamiento activado con el clic derecho
    int lockedIndex = -1;     // objetivo al que está fijada la cámara (-1 = ninguno)

    // Dos paredes sencillas, más altas que el personaje: bloquean el paso, las pokéballs y la luz.
    explicit Scene(const GameData& gameData = GameData::empty(), unsigned worldSeed = std::random_device{}()) : m_data(&gameData) {
        habitats.generate(gameData, worldSeed);
        weather.setup(gameData);
        inventory.setData(gameData);
        storage.setData(gameData);
        chests.setup(ChestSpawn::COUNT);
        nodes.setup(ResourceSpawn::COUNT);
        groundItems.setup(GroundSpawn::COUNT);
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
        if (input.ballSwitch != 0) {
            if (m_aiming) inventory.cycle(input.ballSwitch);
            else storage.cycleActive(input.ballSwitch); // fuera del modo captura cambia el pokémon que acompaña
        }
        if (input.teamSelect > 0) storage.setActive(input.teamSelect - 1); // la misma tecla que el que ya está fuera no cambia nada

        if (input.lockCancel) lockedIndex = -1;
        else if (input.lockTap) cycleLock();
        validateLock(dt);

        camera.update(dt, m_aiming);
        if (lockedIndex >= 0) camera.trackToward(player.body.position, targets[lockedIndex].center(), dt);
        else camera.rotate(input.lookX, input.lookY);

        // Los pokémon son sólidos para quien los pisa: el jugador los rodea igual que a las paredes.
        world.creatures.clear();
        for (const CaptureTarget& target : targets) if (target.hittable()) world.creatures.push_back(target.solid());
        world.props.clear();
        for (const Chest& chest : chests.entities) if (chest.closed()) world.props.push_back(chest.solid());
        for (const ResourceNode& node : nodes.entities) if (!node.depleted()) world.props.push_back(node.solid());
        world.props.push_back(machine.solid());

        player.aiming = m_aiming;
        player.update(dt, input, camera.yaw(), world);

        weather.update(dt);
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
        groundItems.update(dt, GroundSpawn::DELAY,
            [&](GroundItem& item) { world.step(item.body, dt); item.update(dt); },
            [&](int spot) { return GroundSpawn::make(*m_data, spot); });
        updateCompanion(dt);
        updateInteraction();
        if (input.interact) interact();
        updateAimInfo();
        return pause;
    }

    const GameData& data() const { return *m_data; }

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
        return camera.viewMatrix(player.body.position);
    }

    GameStatus status() const {
        GameStatus s;
        s.aimBlend = camera.aimBlend();
        s.playerLevel = progress.level();
        s.playerXp = progress.xpFraction();
        s.levelCap = progress.levelCap();
        s.notice = noticeTime > 0.0f ? notice : Notice::NONE;
        s.locked = lockedIndex >= 0;
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
        s.locationText = locationText();
        if (const OwnedPokemon* lead = storage.active()) s.companionMode = CompanionRules::label(lead->mode);
        addNameTags(s);
        for (size_t i = 0; i < storage.team().size(); ++i) {
            const OwnedPokemon& owned = storage.team()[i];
            if (const PokemonSpecies* species = m_data->speciesById(owned.speciesId)) {
                s.team.push_back({ species, owned.level, owned.shiny, owned.ballId, static_cast<int>(i) == storage.activeIndex() });
            }
        }
        if (!noticeText.empty()) s.noticeText = &noticeText;
        if (m_interaction.kind == Interaction::CHEST) {
            s.interactVerb = CHEST_VERB;
            s.interactTarget = &m_data->chests[chests.entities[m_interaction.index].typeIndex()].name;
        } else if (m_interaction.kind == Interaction::NODE) {
            const ResourceNode& node = nodes.entities[m_interaction.index];
            const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
            const Skill* skill = node.growing() ? m_data->skill(type.skillId) : nullptr;
            s.interactVerb = skill ? skill->name.c_str() : type.action.c_str();
            s.interactTarget = &type.name;
        } else if (m_interaction.kind == Interaction::GROUND) {
            s.interactVerb = PICK_VERB;
            s.interactTarget = &GROUND_NAME;
        } else if (m_interaction.kind == Interaction::MACHINE) {
            s.interactVerb = USE_VERB;
            s.interactTarget = &MACHINE_NAME;
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
    // "Pradera · Arcoíris": hábitat que más pesa donde está el jugador y el clima más fuerte.
    std::string locationText() const {
        const WildSpawn::Context context = spawnContext(player.body.position.x, player.body.position.z);
        if (context.habitats.empty()) return {};
        const int habitat = static_cast<int>(std::max_element(context.habitats.begin(), context.habitats.end()) - context.habitats.begin());
        const Weather* current = context.weather.empty() ? nullptr : m_data->weather(context.weather.front().weatherId);
        return m_data->habitats[habitat].name + " · " + (current ? current->name : CLEAR_SKY);
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
        for (const CaptureTarget& target : targets) {
            if (target.hittable()) add(target.speciesId(), target.level(), target.shiny(), { target.body.position.x, target.body.position.y + CaptureTarget::SIZE, target.body.position.z }, target.hpFraction());
        }
        if (companion.active() && !companion.changing() && storage.active()) {
            const OwnedPokemon& lead = *storage.active();
            add(companion.speciesId(), lead.level, lead.shiny, { companion.body.position.x, companion.body.position.y + Companion::SIZE, companion.body.position.z });
        }
    }

    // Punto del mundo sobre lo que se interactúa, donde se dibuja la ayuda de cómo hacerlo.
    DirectX::XMFLOAT3 interactionAnchor() const {
        const auto above = [](const Physics::Body& body, float height) { return DirectX::XMFLOAT3{ body.position.x, body.position.y + height, body.position.z }; };
        switch (m_interaction.kind) {
            case Interaction::CHEST:   return above(chests.entities[m_interaction.index].body, Chest::HEIGHT + 0.5f);
            case Interaction::NODE:    return nodeAnchor(nodes.entities[m_interaction.index]);
            case Interaction::GROUND:  return above(groundItems.entities[m_interaction.index].body, 0.9f);
            case Interaction::MACHINE: return above(machine.body, ResearchMachine::HEIGHT + 0.5f);
            default:                   return {};
        }
    }

    static DirectX::XMFLOAT3 nodeAnchor(const ResourceNode& node) {
        const float height = (std::min)(ResourceStyle::look(node.typeId()).height, 1.6f) + 0.5f; // los árboles altos: a mano del jugador
        return { node.body.position.x, node.body.position.y + height, node.body.position.z };
    }

    // --- Porcentaje de captura ---
    bool isBehind(const CaptureTarget& t) const {
        return CaptureRules::isBehind(t.body.position, t.yaw(), player.body.position);
    }

    float chancePercent(const CaptureTarget& t, float ballMultiplier) const {
        const PokemonSpecies* species = m_data->speciesById(t.speciesId());
        const float base = species ? CaptureRules::basePercent(species->catchRate, t.level(), progress.level()) * CaptureRules::hpFactor(t.hpFraction()) : 0.0f;
        return CaptureRules::percent(base, isBehind(t), player.crouched(), ballMultiplier);
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
        if (const PokemonSpecies* species = m_data->speciesById(target.speciesId())) {
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
        if (const int levels = progress.addXp(Xp::CAPTURE); levels > 0) text += "   ¡Nivel " + std::to_string(progress.level()) + "!";
        switch (target.throwKind()) {
            case CaptureRules::Throw::LUCKY:       showNotice(Notice::LUCKY, text); break;
            case CaptureRules::Throw::SUPER_LUCKY: showNotice(Notice::SUPER_LUCKY, text); break;
            default:                               showNotice(Notice::CAPTURED, text); break;
        }
    }

    // Hábitat, clima y hora de un punto del mundo: lo que decide qué pokémon aparecen ahí.
    WildSpawn::Context spawnContext(float x, float z) const {
        WildSpawn::Context context;
        habitats.weights(x, z, context.habitats);
        context.weather = weather.at(context.habitats);
        context.night = dayCycle.isNight();
        return context;
    }

    // Especie de un pokémon salvaje que acaba de aparecer, según el hábitat, el clima y la hora de donde sale.
    void assignSpecies(CaptureTarget& target) const {
        const int index = WildSpawn::pick(*m_data, spawnContext(target.body.position.x, target.body.position.z));
        if (index >= 0) target.setSpecies(index, m_data->species[index].id, PokemonRules::rollLevel(m_data->species[index]), PokemonRules::rollShiny());
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
            companion.swap(species->id, m_data->levitates(*species), PokeballStyle::color(lead->ballId), player.body.position, camera.yaw());
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
        const CompanionRules::Mode mode = lead ? lead->mode : CompanionRules::Mode::COLLECT;
        if (species && mode == CompanionRules::Mode::COLLECT) {
            for (ResourceNode& node : nodes.entities) {
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
        const float reach = nodeTarget ? Companion::WORK_RANGE + ResourceStyle::look(nodeTarget->typeId()).half :
                            fightTarget ? CompanionRules::ATTACK_RANGE : Companion::WORK_RANGE;
        const bool arrived = goal && best <= reach * reach;
        if (nodeTarget && arrived) nodeTarget->companionWorking();
        const float teamBonus = nodeTarget && arrived ? nodeTarget->teamBonus(false) : 1.0f;
        const float power = arrived ? Companion::workPower(fightTarget ? 1 : level, teamBonus) : 0.0f;
        if (!companion.update(dt, world, player.body.position, camera.yaw(), power, goal)) return;
        if (nodeTarget) workNode(*nodeTarget, false);
        else if (groundTarget) collect(*groundTarget);
        else if (fightTarget) strike(*fightTarget);
    }

    // El salvaje más cercano al jugador (dentro del radio de búsqueda) al que el acompañante debe atacar según su modo: en captura
    // solo mientras le quede más vida que el límite; en combate, cualquiera.
    CaptureTarget* fightTargetFor(CompanionRules::Mode mode) {
        CaptureTarget* best = nullptr;
        float bestDistance = Companion::SEARCH_RANGE * Companion::SEARCH_RANGE;
        for (CaptureTarget& target : targets) {
            if (!target.hittable() || target.speciesIndex() < 0) continue;
            if (mode == CompanionRules::Mode::CAPTURE && target.hpFraction() <= CompanionRules::CAPTURE_STOP_HP) continue;
            const float dx = target.body.position.x - player.body.position.x, dz = target.body.position.z - player.body.position.z;
            if (dx * dx + dz * dz < bestDistance) {
                bestDistance = dx * dx + dz * dz;
                best = &target;
            }
        }
        return best;
    }

    // Un golpe del acompañante. Si derrota al salvaje, este gana experiencia (y puede subir de nivel o evolucionar).
    void strike(CaptureTarget& target) {
        const OwnedPokemon* lead = storage.active();
        const PokemonSpecies* attacker = lead ? m_data->speciesById(lead->speciesId) : nullptr;
        const PokemonSpecies* defender = m_data->speciesById(target.speciesId());
        if (!attacker || !defender) return;

        const std::string name = attacker->name;
        const int level = lead->level;
        if (!target.damage(CompanionRules::damageFraction(level, attacker->stats.attack, target.level(), defender->stats.defense))) return;

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
        if (const int i = nearestIn(chests.entities, [](const Chest& c) { return c.closed(); }, best); i >= 0) m_interaction = { Interaction::CHEST, i };
        if (const int i = nearestIn(groundItems.entities, [](const GroundItem& g) { return g.available(); }, best); i >= 0) m_interaction = { Interaction::GROUND, i };
        const float mx = machine.body.position.x - player.body.position.x, mz = machine.body.position.z - player.body.position.z;
        if (const float d2 = mx * mx + mz * mz; d2 <= machine.reach() * machine.reach() && d2 < best) {
            best = d2;
            m_interaction = { Interaction::MACHINE, 0 };
        }
        if (const int i = nearestIn(nodes.entities, [](const ResourceNode& n) { return !n.depleted(); }, best); i >= 0) {
            const ResourceNode& node = nodes.entities[i];
            if (playerCanWork(node)) m_interaction = { Interaction::NODE, i };
            else {
                const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
                m_missingSkill = type.skillId; // sin herramienta de ese nivel: solo un pokémon que llegue al nivel puede trabajarlo
                m_missingLevel = type.level;
                m_missingPos = nodeAnchor(node);
            }
        }
    }

    void interact() {
        if (m_interaction.kind == Interaction::CHEST) openChest(chests.entities[m_interaction.index]);
        else if (m_interaction.kind == Interaction::GROUND) collect(groundItems.entities[m_interaction.index]);
        else if (m_interaction.kind == Interaction::MACHINE) researchRequested = true;
        else if (m_interaction.kind == Interaction::NODE) {
            ResourceNode& node = nodes.entities[m_interaction.index];
            const bool tool = node.growing() || !node.plant();
            workNode(node, true, tool ? inventory.skillSpeed(m_data->nodes[node.typeIndex()].skillId) : 1.0f);
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
        if (!node.work(byPlayer, speed) || !node.depleted()) return;
        const ResourceNodeType& type = m_data->nodes[node.typeIndex()];
        if (const Item* item = m_data->materialItem(type.materialId)) giveReward(item->id, RandomUtil::integer(type.minYield, type.maxYield));
        gainXp(Xp::GATHER);
    }

    // Recoge un objeto suelto del mundo.
    void collect(GroundItem& item) {
        if (!item.available()) return;
        if (const GroundItemType* reward = GroundSpawn::pickReward(*m_data)) {
            giveReward(reward->itemId, RandomUtil::integer(reward->minQuantity, reward->maxQuantity));
            gainXp(Xp::PICK_UP);
        }
        item.take();
    }

    // Experiencia del jugador por una acción en el mundo. Si sube de nivel lo avisa (salvo que ya haya un aviso de recompensa).
    void gainXp(int amount) {
        if (progress.addXp(amount) > 0) showNotice(Notice::LEVEL_UP, "Nivel " + std::to_string(progress.level()));
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
    std::vector<int> lockCandidates(int exclude) const {
        std::vector<int> result;
        for (int i = 0; i < TARGET_COUNT; ++i) {
            if (i != exclude && targets[i].hittable() && distanceXZ(targets[i]) <= RANGE && visible(targets[i])) result.push_back(i);
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

    // Si el objetivo fijado se captura, queda fuera de alcance o lleva un rato tapado, salta a otro visible; si no hay, se suelta.
    void validateLock(float dt) {
        if (lockedIndex < 0) return;
        const CaptureTarget& t = targets[lockedIndex];
        if (t.hittable() && distanceXZ(t) <= RANGE + LOCK_RELEASE_MARGIN) {
            m_lockHidden = visible(t) ? 0.0f : m_lockHidden + dt;
            if (m_lockHidden < LOCK_HIDDEN_GRACE) return;
        }
        m_lockHidden = 0.0f;

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
            if (world.blocked(eye, targets[i].center())) continue; // tapado por un obstáculo
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
        ball.typeId = type.id;
        ball.captureMultiplier = type.captureMultiplier;
        ball.color = PokeballStyle::color(type.id);
        ball.body.position = { p.x + cosYaw * SPAWN_SIDE + sinYaw * SPAWN_FORWARD,
                               p.y + SPAWN_HEIGHT * player.heightScale(),
                               p.z - sinYaw * SPAWN_SIDE + cosYaw * SPAWN_FORWARD };
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
                ball.age += h;
                for (CaptureTarget& target : targets) {
                    if (!target.hitBy(ball.body.position, Pokeball::RADIUS + Pokeball::CONTACT_MARGIN)) continue;
                    target.beginCapture(ball, CaptureRules::roll(chancePercent(target, ball.captureMultiplier)));
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
        enum Kind { NONE, CHEST, NODE, GROUND, MACHINE } kind = NONE;
        int index = -1;
    };

    // Hay algún pokémon al alcance al que fijar la cámara (o ya hay uno fijado).
    bool anyLockable() const {
        return lockedIndex >= 0 || !lockCandidates(-1).empty();
    }

    static constexpr const char* CHEST_VERB = "Abrir";
    static constexpr const char* PICK_VERB = "Recoger";
    static constexpr const char* USE_VERB = "Usar";
    inline static const std::string GROUND_NAME = "objeto brillante";
    inline static const std::string MACHINE_NAME = "máquina de investigación";

    const GameData* m_data;
    int m_shownUid = 0;        // pokémon (uid y especie) que está fuera o saliendo
    int m_shownSpecies = -1;
    int m_missingSkill = -1;   // habilidad que falta para el recurso cercano (-1 = ninguna)
    DirectX::XMFLOAT3 m_missingPos = {};
    int m_missingLevel = 0;    // nivel que exige ese recurso
    Interaction m_interaction; // lo que el jugador puede usar ahora mismo (cofre o recurso cercano)
    bool m_aiming = false; // modo lanzamiento (clic derecho) o L2 mantenido
    float m_lockHidden = 0.0f; // segundos que lleva tapado el pokémon fijado
    int m_aimTarget = -1;  // pokémon al que apunta la cruceta dentro del alcance
};
