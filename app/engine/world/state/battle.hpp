#pragma once
#include <deque>
#include <string>
#include <vector>
#include "../../../utils/core/crashScope.hpp"
#include "../rules/battleRules.hpp"
#include "../rules/experienceRules.hpp"

// Combate por turnos 1 contra 1 o 2 contra 2 (dobles) con cambios de pokémon (única definición; no sabe nada de pantallas). El jugador manda una orden por turno
// (submit); el combate resuelve el turno entero y lo cuenta como una cola de mensajes, cada uno con una foto de los PS en ese momento, para
// que la interfaz los reproduzca uno a uno.
class Battle {
    public:
    using Fighter = BattleRules::Fighter;
    using Stat = EffectSpec::Stat;
    using Status = EffectSpec::Status;
    using Weather = EffectSpec::Weather;

    // Cada lado tiene hasta dos huecos en el campo: hueco = lado * 2 + posición (0, 1 = jugador; 2, 3 = rival). En un combate individual solo se usan el 0 y el 2.
    static constexpr int SLOTS = 4;
    static constexpr int slotOf(int side, int pos) { return side * 2 + pos; }
    static constexpr int sideOf(int slot) { return slot / 2; }
    static constexpr int posOf(int slot) { return slot % 2; }

    enum class Phase { CHOOSE, FORCED_SWITCH, OVER };
    enum class Outcome { NONE, WON, LOST };

    struct Action {
        enum class Kind { MOVE, SWITCH } kind = Kind::MOVE;
        int index = 0;    // hueco del movimiento o posición en el equipo
        int target = -1;  // (dobles) hueco rival al que se ataca; -1 = el primero que quede en pie
    };

    // Un mensaje de la cola y cómo estaba la escena al darlo (por hueco del campo).
    struct Message {
        std::string text;
        int speciesId[SLOTS] = { -1, -1, -1, -1 };
        std::string name[SLOTS];
        int level[SLOTS] = { 1, 1, 1, 1 };
        int hp[SLOTS] = { 0, 0, 0, 0 };
        int maxHp[SLOTS] = { 1, 1, 1, 1 };
        Status status[SLOTS] = { Status::NONE, Status::NONE, Status::NONE, Status::NONE };
        bool shiny[SLOTS] = { false, false, false, false };
        int attacker = -1;  // hueco que ataca en este mensaje (-1 = ninguno)
        int target = -1;    // hueco que recibe el golpe
        int moveType = -1;  // tipo del movimiento (para el efecto visual)
        bool hit = false;   // el golpe llegó (el defensor se sacude)
        int types[SLOTS][2] = { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } };
        int stages[SLOTS][static_cast<int>(Stat::COUNT)] = {};
        int fxSide = -1;      // hueco al que le pasa algo visible (sube/baja una estadística, se activa una habilidad)
        int fxKind = 0;       // 0 nada, 1 sube, 2 baja, 3 habilidad
        int fxStat = -1;
        int fxAmount = 0;
        std::string fxText;
        bool onField[SLOTS] = { false, false, false, false }; // cada pokémon está ya en el campo
        int animKind = 0;     // 1 vuelve a su pokéball, 2 sale de su pokéball, 3 se debilita
        int animSide = -1;    // hueco de esa animación
    };

    Battle(const GameData& data, std::vector<Fighter> player, int playerActive, std::vector<Fighter> foe, std::string foeName, bool doubles = false)
        : m_data(&data), m_foeName(std::move(foeName)), m_doubles(doubles) {
        m_team[0] = std::move(player);
        m_team[1] = std::move(foe);
        if (playerActive < 0 || playerActive >= static_cast<int>(m_team[0].size()) || m_team[0][static_cast<size_t>(playerActive)].fainted()) playerActive = nextReserve(0);
        m_active[0] = playerActive;
        m_active[2] = nextReserve(1);
        if (m_doubles) {
            m_active[1] = nextReserve(0);
            m_active[3] = nextReserve(1);
        }
        say(m_foeName + " quiere combatir");
        for (const int slot : { 2, 3, 0, 1 }) if (m_active[slot] >= 0) enter(slot, false); // primero salen todos los pokémon...
        for (const int slot : { 2, 3, 0, 1 }) if (m_active[slot] >= 0) triggerEntry(slot);  // ...y entonces actúan las habilidades de entrada (Intimidación...)
    }

    Phase phase() const { return m_phase; }
    Outcome outcome() const { return m_outcome; }
    bool doubles() const { return m_doubles; }
    const std::vector<Fighter>& team(int side) const { return m_team[side]; }
    int active(int slot) const { return m_active[slot]; }                  // posición en el equipo del que ocupa ese hueco (-1 = vacío)
    bool occupied(int slot) const { return m_active[slot] >= 0; }
    const Fighter& fighter(int slot) const { return m_team[sideOf(slot)][static_cast<size_t>(m_active[slot] >= 0 ? m_active[slot] : 0)]; } // pensado para occupied(slot); sin pokémon en el hueco devuelve el primero del equipo en vez de salirse del vector
    bool inField(int slot) const { return occupied(slot) && !fighter(slot).fainted(); }
    const std::string& foeName() const { return m_foeName; }
    const std::vector<int>& defeatedLevels() const { return m_defeated; } // niveles de los pokémon rivales derrotados (el jugador gana experiencia por ellos)

    // Cola de mensajes pendientes de mostrar.
    bool hasMessage() const { return !m_messages.empty(); }
    const Message& front() const { return m_messages.front(); }
    void pop() { if (!m_messages.empty()) m_messages.pop_front(); }
    // Hasta que se vacía la cola no se aceptan órdenes.
    bool ready() const { return m_messages.empty() && m_phase != Phase::OVER; }

    // Huecos del jugador que deben recibir una orden este turno (los que tienen un pokémon en pie), y los rivales que se pueden atacar.
    std::vector<int> playerSlots() const { return aliveSlots(0); }
    std::vector<int> foeSlots() const { return aliveSlots(1); }

    // ¿Puede ese hueco usar ese movimiento (tiene movimiento y PP)? Sin ninguno usable, el 0 es Forcejeo.
    // Un oxidado no puede repetir el movimiento del turno anterior (salvo que no le quede otro).
    bool canUse(int slot, int move) const {
        if (!inField(slot)) return false;
        const Fighter& f = fighter(slot);
        if (!(move >= 0 && move < MoveRules::MAX_KNOWN && f.moves[static_cast<size_t>(move)] >= 0 && f.pp[static_cast<size_t>(move)] > 0)) return false;
        if (f.status == Status::RUSTED && f.lastMove == move) for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) if (i != move && f.moves[static_cast<size_t>(i)] >= 0 && f.pp[static_cast<size_t>(i)] > 0) return false;
        return true;
    }
    // Un enraizado no puede retirarse del campo.
    bool canSwitchOut(int slot) const { return inField(slot) && fighter(slot).status != Status::ROOTED; }
    Weather weather() const { return m_weather; }
    bool mustStruggle(int slot) const { return inField(slot) && !fighter(slot).usable(); }
    // ¿Hay que elegir a quién va el movimiento? Siempre si puede ir a cualquiera (rival, aliado o uno mismo; también en individuales y con un solo rival);
    // si no, solo en dobles cuando ataca al rival y quedan dos rivales en pie.
    bool needsTarget(int slot, int move) const {
        if (!inField(slot)) return false;
        const Move* m = mustStruggle(slot) ? nullptr : moveOf(fighter(slot), move);
        if (m && m->scope == Move::Scope::ANY) return true; // aliado, uno mismo o rival
        if (!m_doubles) return false;
        if (foeSlots().size() < 2) return false;
        if (mustStruggle(slot)) return true;
        if (!m) return false;
        if (m->scope == Move::Scope::ALL_OTHERS || m->scope == Move::Scope::FOES) return false; // alcanzan a varios: no se elige
        return m->scope == Move::Scope::ANY || targetsFoeOf(*m);
    }
    // Huecos a los que puede apuntar ese movimiento: los rivales y, si su alcance lo permite (Move::Scope::ANY), también el aliado y él mismo.
    std::vector<int> targetChoices(int slot, int move) const {
        std::vector<int> list = foeSlots();
        const Move* m = mustStruggle(slot) ? nullptr : moveOf(fighter(slot), move);
        if (m && m->scope == Move::Scope::ANY) for (int other = 0; other < SLOTS; ++other) if (sideOf(other) == sideOf(slot) && inField(other)) list.push_back(other);
        return list;
    }
    // Cambio a ese pokémon del equipo: en pie y fuera del campo ('reserved' = el que ya va a sacar el otro hueco este turno).
    bool canSwitchTo(int index, int reserved = -1) const {
        if (index < 0 || index >= static_cast<int>(m_team[0].size()) || index == reserved || m_team[0][static_cast<size_t>(index)].fainted()) return false;
        return index != m_active[0] && index != m_active[1];
    }
    // Hueco que espera pokémon nuevo tras caer el suyo (-1 = ninguno).
    int pendingSlot() const { return m_pending.empty() ? -1 : m_pending.front(); }

    // Cambio obligatorio tras caer un pokémon del jugador.
    bool forceSwitch(int index) {
        if (m_phase != Phase::FORCED_SWITCH || !ready() || m_pending.empty() || !canSwitchTo(index)) return false;
        const int slot = m_pending.front();
        m_pending.erase(m_pending.begin());
        switchTo(slot, index);
        m_replaced[slot] = true;
        if (nextReserve(0) < 0) { // sin más relevos, el otro hueco que esperaba se queda vacío
            for (const int waiting : m_pending) { m_active[waiting] = -1; m_onField[waiting] = false; }
            m_pending.clear();
        }
        if (m_pending.empty()) m_phase = Phase::CHOOSE;
        return true;
    }

    // Manda las órdenes del jugador (una por cada hueco de playerSlots(), en ese orden) y resuelve el turno (el rival decide solo).
    bool submit(const std::vector<Action>& actions) {
        CRASH_SCOPE();
        if (m_phase != Phase::CHOOSE || !ready()) return false;
        const std::vector<int> mine = playerSlots();
        if (actions.size() != mine.size() || mine.empty()) return false;
        int reserved = -1;
        for (size_t i = 0; i < mine.size(); ++i) {
            const Action& a = actions[i];
            if (a.kind == Action::Kind::MOVE && !mustStruggle(mine[i]) && !canUse(mine[i], a.index)) return false;
            if (a.kind == Action::Kind::SWITCH) {
                if (!canSwitchTo(a.index, reserved) || !canSwitchOut(mine[i])) return false;
                reserved = a.index;
            }
        }
        std::vector<Order> order;
        for (size_t i = 0; i < mine.size(); ++i) order.push_back({ mine[i], m_active[mine[i]], actions[i] });
        for (const int slot : foeSlots()) order.push_back({ slot, m_active[slot], chooseFoe(slot) });
        for (Order& o : order) o.claw = quickClaw(o.slot); // se tira una sola vez por pokémon (un comparador no puede ser aleatorio)
        std::sort(order.begin(), order.end(), [&](const Order& a, const Order& b) { return before(a, b); });
        for (Fighter& f : m_team[0]) f.protecting = false;
        for (Fighter& f : m_team[1]) f.protecting = false;
        for (int slot = 0; slot < SLOTS; ++slot) if (occupied(slot)) cur(slot).flinched = false;
        for (bool& r : m_replaced) r = false;
        for (const Order& o : order) {
            if (m_phase == Phase::OVER) break;
            if (m_replaced[o.slot] || m_active[o.slot] != o.member) continue; // el que entró (o salió) a mitad de turno no actúa
            if (o.action.kind == Action::Kind::SWITCH) { if (!cur(o.slot).fainted() && canSwitchTo(o.action.index)) switchTo(o.slot, o.action.index); continue; }
            if (cur(o.slot).fainted()) continue; // el que cae a mitad de turno no actúa
            const int target = resolveTarget(o.slot, o.action);
            if (target < 0) continue;
            act(o.slot, o.action.index, target);
        }
        if (m_phase != Phase::OVER) endTurn();
        if (m_phase != Phase::OVER) resolveFaints();
        return true;
    }
    // Combate individual: una sola orden.
    bool submit(const Action& action) { return submit(std::vector<Action>{ action }); }

    // Copia de vuelta lo que cambió (PS, PP) para cada pokémon del jugador, por su posición de origen.
    const std::vector<Fighter>& playerTeam() const { return m_team[0]; }

    // Los transformados vuelven a ser lo que eran (así sus movimientos y PP se guardan bien).
    void restoreForms() { for (int side = 0; side < 2; ++side) for (Fighter& f : m_team[side]) untransform(f); }

    private:
    struct Order { int slot; int member; Action action; bool claw = false; };

    const GameData* m_data;
    std::vector<Fighter> m_team[2];
    int m_active[SLOTS] = { -1, -1, -1, -1 };
    std::string m_foeName;
    bool m_doubles = false;
    std::deque<Message> m_messages;
    Phase m_phase = Phase::CHOOSE;
    Outcome m_outcome = Outcome::NONE;
    std::vector<int> m_defeated;
    std::vector<int> m_pending; // huecos del jugador que esperan sustituto
    bool m_onField[SLOTS] = { false, false, false, false };
    bool m_counted[SLOTS] = { false, false, false, false };  // rival caído cuya experiencia ya se repartió
    bool m_replaced[SLOTS] = { false, false, false, false };
    bool m_coin = true;
    Weather m_weather = Weather::NONE;
    int m_weatherTurns = 0;

    // --- Utilidades ---
    int nextReserve(int side) const { // primer pokémon en pie del equipo que no está ya en el campo
        for (int i = 0; i < static_cast<int>(m_team[side].size()); ++i) {
            if (m_team[side][static_cast<size_t>(i)].fainted() || i == m_active[side * 2] || i == m_active[side * 2 + 1]) continue;
            return i;
        }
        return -1;
    }
    bool anyAlive(int side) const { for (const Fighter& f : m_team[side]) if (!f.fainted()) return true; return false; }
    std::vector<int> aliveSlots(int side) const {
        std::vector<int> list;
        for (int pos = 0; pos < 2; ++pos) if (inField(slotOf(side, pos))) list.push_back(slotOf(side, pos));
        return list;
    }
    Fighter& cur(int slot) { return m_team[sideOf(slot)][static_cast<size_t>(m_active[slot])]; }
    const Fighter& cur(int slot) const { return m_team[sideOf(slot)][static_cast<size_t>(m_active[slot])]; }
    static std::string who(const Fighter& f, int slot) { return (sideOf(slot) == 1 ? "El " : "") + f.name + (sideOf(slot) == 1 ? " rival" : ""); }
    // Primer rival en pie de ese hueco (para habilidades y efectos sin objetivo elegido); -1 si no queda ninguno.
    int opponentOf(int slot) const { const std::vector<int> list = aliveSlots(1 - sideOf(slot)); return list.empty() ? -1 : list.front(); }

    void say(const std::string& text, int attacker = -1, int moveType = -1, bool hit = false, int target = -1) {
        Message m;
        m.text = text;
        m.attacker = attacker;
        m.target = target;
        m.moveType = moveType;
        m.hit = hit;
        for (int slot = 0; slot < SLOTS; ++slot) {
            m.onField[slot] = m_onField[slot] && m_active[slot] >= 0;
            if (m_active[slot] < 0) continue;
            const Fighter& f = cur(slot);
            m.speciesId[slot] = f.speciesId; m.name[slot] = f.name; m.level[slot] = f.level; m.hp[slot] = f.hp; m.maxHp[slot] = f.maxHp;
            m.status[slot] = f.status; m.shiny[slot] = f.shiny;
            for (int i = 0; i < 2; ++i) m.types[slot][i] = i < static_cast<int>(f.types.size()) ? f.types[static_cast<size_t>(i)] : -1;
            for (int i = 0; i < static_cast<int>(Stat::COUNT); ++i) m.stages[slot][i] = f.stages[static_cast<size_t>(i)];
        }
        m_messages.push_back(m);
    }

    // El último mensaje lleva una animación de pokéball (1 vuelve, 2 sale, 3 se debilita) para ese hueco.
    void anim(int slot, int kind) { m_messages.back().animKind = kind; m_messages.back().animSide = slot; }

    // Marca el último mensaje con un aviso visual para 'slot' (sube/baja una estadística, habilidad activada).
    void tag(int slot, int kind, const std::string& text, int stat = -1, int amount = 0) {
        Message& m = m_messages.back();
        m.fxSide = slot; m.fxKind = kind; m.fxText = text; m.fxStat = stat; m.fxAmount = amount;
    }

    bool chance(float p) const { return RandomUtil::range(0.0f, 1.0f) < p; }

    const Move* moveOf(const Fighter& f, int slot) const { return slot >= 0 && slot < MoveRules::MAX_KNOWN ? m_data->move(f.moves[static_cast<size_t>(slot)]) : nullptr; }
    static bool targetsFoeOf(const Move& move) { return (move.damaging() || !move.effect.self) && move.effect.kind != EffectSpec::Kind::PROTECT; }

    // Forcejeo: lo que usa quien se queda sin PP (sin tipo, con retroceso).
    const Move& struggle() const {
        static Move move = [] {
            Move m; m.id = -2; m.name = "Forcejeo"; m.typeId = -1; m.kind = MoveClass::Kind::PHYSICAL; m.power = BattleRules::STRUGGLE_POWER; m.contact = true;
            m.effect.kind = EffectSpec::Kind::RECOIL; m.effect.self = true; m.effect.amount = 25.0f;
            return m;
        }();
        return move;
    }

    // Marca qué pokémon del jugador han estado frente a cada rival (para repartir la experiencia al derrotarlo).
    void markFaced() {
        for (const int foe : { 2, 3 }) {
            if (!inField(foe)) continue;
            for (const int mine : { 0, 1 }) if (m_active[mine] >= 0) cur(foe).faced |= 1u << m_active[mine];
        }
    }

    // --- Entrada en combate y cambios ---
    void enter(int slot, bool abilities = true) {
        Fighter& f = cur(slot);
        if (sideOf(slot) == 0) f.participated = true;
        m_onField[slot] = true;
        m_counted[slot] = false;
        say(sideOf(slot) == 0 ? "¡Adelante, " + f.name + "!" : m_foeName + " envía a " + f.name + ".");
        anim(slot, 2);
        markFaced();
        if (abilities) triggerEntry(slot);
    }
    // Habilidades que actúan al entrar en combate: las que afectan al rival lo hacen con todos los rivales que haya en el campo (en dobles, los dos).
    void triggerEntry(int slot, bool copied = false) {
        if (!inField(slot)) return;
        Fighter& f = cur(slot);
        if (!copied && BattleRules::hasKind(*m_data, f, EffectSpec::Kind::ILLUSION)) f.illusion = true;
        for (const Ability* a : BattleRules::abilitiesOf(*m_data, f)) {
            if (a->trigger != Ability::Trigger::ON_ENTER || !a->spec.active()) continue;
            if (copied && a->spec.kind == EffectSpec::Kind::TRACE) continue;
            say(f.name + " activa " + a->name);
            tag(slot, 3, a->name);
            if (special(a->spec, slot)) continue;
            if (a->spec.self) { applyEffect(a->spec, slot, slot, 0, nullptr, true); continue; }
            for (int other = 0; other < SLOTS; ++other) if (sideOf(other) != sideOf(slot) && inField(other)) applyEffect(a->spec, slot, other, 0, nullptr);
        }
    }

    // Habilidades de un momento concreto (golpe recibido, derrotar a un rival, bajada de estadística...) del pokémon de ese hueco; 'other' = el pokémon implicado (-1 = el primer rival).
    void fireTrigger(int slot, Ability::Trigger trigger, int other) {
        if (!inField(slot)) return;
        for (const Ability* a : BattleRules::abilitiesOf(*m_data, cur(slot))) {
            if (a->trigger != trigger || !a->spec.active() || !inField(slot)) continue;
            if (a->spec.chance < 100.0f && !chance(a->spec.chance / 100.0f)) continue;
            const int partner = other >= 0 ? other : opponentOf(slot);
            say(cur(slot).name + " activa " + a->name);
            tag(slot, 3, a->name);
            if (special(a->spec, slot)) continue;
            if (a->spec.self || partner < 0) applyEffect(a->spec, slot, slot, 0, nullptr, true);
            else applyEffect(a->spec, slot, partner, 0, nullptr, true);
        }
    }

    // Efectos de habilidad que no son un efecto de combate normal (clima, copiar, transformarse...). true = ya lo ha gestionado.
    bool special(const EffectSpec& e, int slot) {
        switch (e.kind) {
            case EffectSpec::Kind::WEATHER: setWeather(e.weather); return true;
            case EffectSpec::Kind::TRACE: {
                const int foe = opponentOf(slot);
                Fighter& f = cur(slot);
                const Ability* copied = foe >= 0 ? m_data->ability(cur(foe).abilityId) : nullptr;
                if (!copied || copied->id == f.abilityId) return true;
                f.abilityId = copied->id;
                say(f.name + " copia " + copied->name + ".");
                triggerEntry(slot, true);
                return true;
            }
            case EffectSpec::Kind::TRANSFORM: transform(slot); return true;
            case EffectSpec::Kind::COPY_ALLY: {
                const int ally = slotOf(sideOf(slot), 1 - posOf(slot));
                if (!inField(ally)) return true;
                cur(slot).stages = cur(ally).stages;
                say(cur(slot).name + " copia las mejoras de " + cur(ally).name + ".");
                return true;
            }
            case EffectSpec::Kind::HEAL_ALLY: {
                const int ally = slotOf(sideOf(slot), 1 - posOf(slot));
                if (!inField(ally)) return true;
                Fighter& a = cur(ally);
                a.hp = (std::min)(a.maxHp, a.hp + (std::max)(1, a.maxHp * static_cast<int>(e.amount) / 100));
                say(who(a, ally) + " recupera PS.");
                return true;
            }
            default: return false;
        }
    }

    // --- Clima ---
    // El clima que cuenta: ninguno si alguien en el campo tiene una habilidad que lo anula (Aclimatación).
    Weather effectiveWeather() const {
        for (int slot = 0; slot < SLOTS; ++slot) if (inField(slot) && BattleRules::hasKind(*m_data, cur(slot), EffectSpec::Kind::WEATHER_NEGATE)) return Weather::NONE;
        return m_weather;
    }
    void setWeather(Weather weather) {
        if (weather == Weather::NONE || weather == m_weather) return;
        m_weather = weather;
        m_weatherTurns = BattleRules::WEATHER_TURNS;
        say(std::string("Empieza ") + (weather == Weather::RAIN ? "a llover" : weather == Weather::SUN ? "un sol intenso" : weather == Weather::SAND ? "una tormenta de arena" : "a nevar") + ".");
    }

    // --- Objetos: un estafado los tiene anulados y se castiga cada vez que intenta usar el equipado ---
    bool swindledWith(const Fighter& f, HeldItem::Effect effect) const {
        if (f.status != Status::SWINDLED) return false;
        const HeldItem* real = m_data->held(f.heldId);
        return real && real->effect == effect;
    }
    void punishItem(int slot) {
        Fighter& f = cur(slot);
        f.hp = (std::max)(0, f.hp - (std::max)(1, f.maxHp / BattleRules::RECOIL_STATUS_DIVISOR));
        say(who(f, slot) + " sufre un retroceso por intentar usar su objeto.");
    }

    // --- Transformación (Impostor) ---
    void transform(int slot) {
        const int foe = opponentOf(slot);
        Fighter& f = cur(slot);
        if (foe < 0 || f.transformed) return;
        const Fighter& o = cur(foe);
        f.origStats = { f.attack, f.spAttack, f.defense, f.spDefense, f.speed };
        f.origMoves = f.moves;
        f.origPp = f.pp;
        f.types = o.types;
        f.attack = o.attack; f.spAttack = o.spAttack; f.defense = o.defense; f.spDefense = o.spDefense; f.speed = o.speed;
        f.stages = o.stages;
        f.moves = o.moves;
        for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) { const Move* m = m_data->move(o.moves[static_cast<size_t>(i)]); f.pp[static_cast<size_t>(i)] = m ? (std::min)(m->pp, BattleRules::TRANSFORM_PP) : 0; }
        f.transformed = true;
        say(f.name + " se transforma en " + o.name + ".");
    }
    void untransform(Fighter& f) {
        if (!f.transformed) return;
        f.transformed = false;
        f.attack = f.origStats[0]; f.spAttack = f.origStats[1]; f.defense = f.origStats[2]; f.spDefense = f.origStats[3]; f.speed = f.origStats[4];
        f.moves = f.origMoves;
        f.pp = f.origPp;
        if (const PokemonSpecies* species = m_data->speciesById(f.speciesId)) f.types = species->types;
    }

    void switchTo(int slot, int index) {
        if (m_active[slot] >= 0) {
            Fighter& old = cur(slot);
            if (!old.fainted()) { say(sideOf(slot) == 0 ? "¡Vuelve, " + old.name + "!" : m_foeName + " retira a " + old.name + "."); anim(slot, 1); }
            old.stages = {};
            old.flinched = false;
            old.protecting = false;
            old.lastMove = -1;
            old.stance = false;
            old.hero = old.hero || BattleRules::hasKind(*m_data, old, EffectSpec::Kind::HERO);
            if (BattleRules::isVolatile(old.status)) { old.status = Status::NONE; old.statusTurns = 0; }
            untransform(old);
        }
        m_onField[slot] = false;
        m_active[slot] = index;
        enter(slot);
    }

    // --- IA del rival: mejor movimiento y objetivo por puntuación ---
    Action chooseFoe(int slot) {
        const Fighter& me = cur(slot);
        const std::vector<int> targets = aliveSlots(0);
        Action best; best.kind = Action::Kind::MOVE; best.index = 0;
        if (targets.empty()) return best;
        best.target = targets.front();
        if (!me.usable()) { best.target = targets[static_cast<size_t>(RandomUtil::integer(0, static_cast<int>(targets.size()) - 1))]; return best; }
        float bestScore = -1.0f;
        std::vector<Action> options;
        for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) {
            const Move* move = moveOf(me, i);
            if (!move || !canUse(slot, i)) continue;
            if (me.status == Status::DESTABILIZED && !move->damaging()) continue; // un desestabilizado no puede usar movimientos de estado
            std::vector<int> candidates = targets;
            if (move->scope == Move::Scope::ANY && move->effect.self) candidates = { slot }; // un movimiento propio que se puede dirigir: a sí mismo
            for (const int t : candidates) {
                const Fighter& you = cur(t);
                float score = 5.0f;
                if (move->damaging()) {
                    const BattleRules::Damage d = BattleRules::damage(*m_data, me, you, *move, false, 0.5f, effectiveWeather());
                    score = static_cast<float>(d.amount) * BattleRules::hitChance(*m_data, me, you, *move, effectiveWeather()) + (d.amount >= you.hp ? 1000.0f : 0.0f);
                } else if (move->effect.kind == EffectSpec::Kind::HEAL_SELF) score = me.hpFraction() < 0.5f ? 80.0f : 0.0f;
                else if (move->effect.kind == EffectSpec::Kind::STATUS && (you.status != Status::NONE || !m_data->statusAffects(move->effect.status, you.types))) score = 0.0f;
                else if (move->effect.kind == EffectSpec::Kind::PROTECT) score = 2.0f;
                score *= RandomUtil::range(0.8f, 1.2f);
                Action a; a.index = i; a.target = t;
                options.push_back(a);
                if (score > bestScore) { bestScore = score; best = a; }
            }
        }
        if (!options.empty() && chance(0.1f)) best = options[static_cast<size_t>(RandomUtil::integer(0, static_cast<int>(options.size()) - 1))]; // a veces improvisa
        if (options.empty()) { best.index = 0; for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) if (moveOf(me, i) && me.pp[static_cast<size_t>(i)] > 0) { best.index = i; break; } }
        return best;
    }

    // --- Orden de ataque ---
    int priorityOf(const Order& o) const {
        if (o.action.kind == Action::Kind::SWITCH) return 100;
        const Fighter& f = cur(o.slot);
        const Move* move = f.usable() ? moveOf(f, o.action.index) : nullptr;
        if (!move) return 0;
        int priority = move->priority;
        if (move->kind == MoveClass::Kind::STATUS) if (const Ability* a = BattleRules::abilityOfKind(*m_data, f, EffectSpec::Kind::PRIORITY_STATUS)) priority += static_cast<int>(a->spec.amount);
        if (f.status == Status::WEIGHED && priority > 0) priority = 0; // lastrado: pierde la prioridad
        return priority;
    }
    bool before(const Order& a, const Order& b) {
        const int pa = priorityOf(a), pb = priorityOf(b);
        if (pa != pb) return pa > pb;
        const float sa = BattleRules::speedOf(*m_data, cur(a.slot), effectiveWeather()), sb = BattleRules::speedOf(*m_data, cur(b.slot), effectiveWeather());
        if (a.claw != b.claw) return a.claw; // Garra rápida: pasa primero dentro de su prioridad
        if (sa != sb) return sa > sb;
        return a.slot < b.slot ? m_coin : !m_coin;
    }
    bool quickClaw(int slot) {
        const HeldItem* held = BattleRules::heldOf(*m_data, cur(slot));
        if (held && held->effect == HeldItem::Effect::QUICK_CLAW) return chance(static_cast<float>(held->amount) / 100.0f);
        if (!held && swindledWith(cur(slot), HeldItem::Effect::QUICK_CLAW)) { if (chance(static_cast<float>(m_data->held(cur(slot).heldId)->amount) / 100.0f)) punishItem(slot); }
        return false;
    }

    // A quién va dirigido de verdad: lo elegido si sigue en pie; si cayó, el otro rival; lo que afecta a uno mismo, él mismo (-1 = no hay a quién).
    int resolveTarget(int slot, const Action& action) const {
        const Fighter& me = cur(slot);
        const Move* move = me.usable() ? moveOf(me, action.index) : &struggle();
        if (!move) return -1;
        const int wanted = action.target;
        if (move->scope == Move::Scope::ANY && wanted >= 0 && wanted < SLOTS && inField(wanted)) return wanted; // aliado, uno mismo o rival
        if (!targetsFoeOf(*move)) return slot;
        if (wanted >= 0 && wanted < SLOTS && sideOf(wanted) != sideOf(slot) && inField(wanted)) return wanted;
        return opponentOf(slot);
    }

    // --- Un movimiento ---
    void faintCheck(int slot) {
        Fighter& f = cur(slot);
        if (f.fainted() && m_onField[slot]) { say(who(f, slot) + " se debilitó."); anim(slot, 3); m_onField[slot] = false; }
    }

    // Un pokémon se hace daño a sí mismo (confusión): golpe sin tipo con su propio ataque.
    void hitSelf(int slot) {
        static const Move move = [] { Move m; m.id = -3; m.name = "Confusión"; m.typeId = -1; m.kind = MoveClass::Kind::PHYSICAL; m.power = BattleRules::CONFUSION_POWER; return m; }();
        Fighter& me = cur(slot);
        const BattleRules::Damage d = BattleRules::damage(*m_data, me, me, move, false, 0.5f, effectiveWeather());
        me.hp = (std::max)(0, me.hp - d.amount);
        say(who(me, slot) + " está confundido: ¡se hirió a sí mismo!", slot, -1, true, slot);
        faintCheck(slot);
    }

    void act(int slot, int moveSlot, int targetSlot) {
        CRASH_SCOPE();
        Fighter& me = cur(slot);
        if (me.flinched) { say(who(me, slot) + " se amedrentó y no pudo moverse."); me.flinched = false; fireTrigger(slot, Ability::Trigger::ON_FLINCH, -1); return; }
        if (me.status == Status::FROZEN) {
            if (chance(BattleRules::FREEZE_THAW)) { me.status = Status::NONE; say(who(me, slot) + " se descongeló."); }
            else { say(who(me, slot) + " está congelado: ¡no se puede mover!"); return; }
        }
        if (me.status == Status::PARALYSIS && chance(BattleRules::PARALYSIS_SKIP)) { say(who(me, slot) + " está paralizado: ¡no se puede mover!"); return; }
        if (me.status == Status::CONFUSED && chance(BattleRules::CONFUSION_SELF_HIT)) { hitSelf(slot); resolveFaints(); return; }
        const bool struggling = !me.usable();
        const Move* chosen = struggling ? &struggle() : moveOf(me, moveSlot);
        if (!chosen) return;
        const Move& move = *chosen;
        if (me.status == Status::DESTABILIZED && !move.damaging()) { say(who(me, slot) + " está desestabilizado: no puede usar movimientos de estado."); return; }
        const std::vector<int> targets = targetsOf(slot, move, targetSlot);
        if (!struggling) {
            int cost = me.status == Status::RUSTED ? 2 : 1; // oxidado: cada movimiento cuesta el doble de PP
            for (const int other : targets) if (other != slot && sideOf(other) != sideOf(slot) && inField(other)) if (const Ability* a = BattleRules::abilityOfKind(*m_data, cur(other), EffectSpec::Kind::PP_DRAIN, BattleRules::breaks(*m_data, me))) cost += static_cast<int>(a->spec.amount);
            me.pp[static_cast<size_t>(moveSlot)] = (std::max)(0, me.pp[static_cast<size_t>(moveSlot)] - cost);
            me.lastMove = moveSlot;
        }
        if (me.status == Status::STUNNED) { // aturdido: cada movimiento le hace daño de retroceso
            me.hp = (std::max)(0, me.hp - (std::max)(1, me.maxHp / BattleRules::RECOIL_STATUS_DIVISOR));
            say(who(me, slot) + " sufre por el aturdimiento.");
            if (me.fainted()) { faintCheck(slot); resolveFaints(); return; }
        }
        const int moveType = BattleRules::moveTypeOf(*m_data, me, move);
        if (BattleRules::hasKind(*m_data, me, EffectSpec::Kind::PROTEAN) && moveType > 0 && !(me.types.size() == 1 && me.types[0] == moveType)) {
            me.types = { moveType };
            const PokemonType* type = m_data->type(moveType);
            say(me.name + " cambia a tipo " + (type ? type->name : std::string("distinto")) + ".");
        }
        if (BattleRules::hasKind(*m_data, me, EffectSpec::Kind::STANCE) && me.stance != move.damaging()) { me.stance = move.damaging(); say(me.name + (me.stance ? " adopta la postura de ataque." : " adopta la postura de defensa.")); }
        const bool targetsFoe = targetsFoeOf(move) || move.scope == Move::Scope::ANY;
        say(who(me, slot) + " usó " + move.name + ".", slot, moveType, false, targetsFoe ? targets.front() : slot);

        if (move.effect.kind == EffectSpec::Kind::PROTECT) {
            const float p = std::pow(1.0f / 3.0f, static_cast<float>(me.protectStreak));
            if (chance(p)) { me.protecting = true; ++me.protectStreak; say(who(me, slot) + " se protege."); }
            else { me.protectStreak = 0; say("¡Pero falló!"); }
            return;
        }
        me.protectStreak = 0;

        for (const int wanted : targets) {
            if (me.fainted() && wanted != slot) break;
            strike(slot, move, targetsFoe ? wanted : slot, targetsFoe);
        }
        if (swindledWith(me, HeldItem::Effect::RAZOR_CLAW) && move.damaging() && !me.fainted()) punishItem(slot);
        faintCheck(slot);
        resolveFaints();
    }

    // Objetivos de un movimiento según su alcance (Move::Scope): uno solo, todos los demás del campo o los dos rivales.
    std::vector<int> targetsOf(int slot, const Move& move, int chosen) const {
        std::vector<int> list;
        if (move.scope == Move::Scope::ALL_OTHERS || move.scope == Move::Scope::FOES) {
            for (int other = 0; other < SLOTS; ++other) {
                if (other == slot || !inField(other)) continue;
                if (move.scope == Move::Scope::FOES && sideOf(other) == sideOf(slot)) continue;
                list.push_back(other);
            }
        }
        if (list.empty()) list.push_back(chosen);
        return list;
    }

    // El movimiento sobre un objetivo: protección, puntería, daño y efecto.
    void strike(int slot, const Move& move, int target, bool targetsFoe) {
        CRASH_SCOPE();
        Fighter& me = cur(slot);
        if (target < 0) return;
        if (targetsFoe && !inField(target)) target = opponentOf(slot); // su objetivo ya cayó este turno
        if (target < 0) return;
        const bool broken = BattleRules::breaks(*m_data, me);
        if (move.kind == MoveClass::Kind::STATUS && targetsFoe && sideOf(target) != sideOf(slot)) { // Espejo mágico: devuelve los movimientos de estado
            if (const Ability* a = BattleRules::abilityOfKind(*m_data, cur(target), EffectSpec::Kind::REFLECT_STATUS, broken)) {
                say(cur(target).name + " activa " + a->name);
                tag(target, 3, a->name);
                target = slot;
            }
        }
        Fighter& foe = cur(target);
        const bool hostile = targetsFoe && target != slot; // cualquier otro pokémon del campo, aliado incluido (Terremoto): la protección lo frena
        if (hostile && foe.protecting) { say(who(foe, target) + " se protegió."); return; }
        if (hostile && !chance(BattleRules::hitChance(*m_data, me, foe, move, effectiveWeather()))) { say("¡" + who(me, slot) + " falló!"); return; }

        int dealt = 0;
        if (move.damaging()) {
            const int moveType = BattleRules::moveTypeOf(*m_data, me, move);
            if (hostile) {
                for (const Ability* a : BattleRules::abilitiesOf(*m_data, foe)) { // Pararrayos...: absorbe los movimientos de su tipo
                    if (broken || a->spec.kind != EffectSpec::Kind::ABSORB_TYPE || a->spec.typeId != moveType) continue;
                    say(foe.name + " absorbe el golpe con " + a->name + ".");
                    tag(target, 3, a->name);
                    changeStage(target, a->spec.stat, static_cast<int>(a->spec.amount), -1);
                    return;
                }
                if (foe.illusion && !broken) { foe.illusion = false; say("¡La ilusión de " + foe.name + " se rompe y el golpe no le alcanza!"); return; }
            }
            const bool crit = BattleRules::isCrit(*m_data, me, foe, move, RandomUtil::range(0.0f, 1.0f));
            const BattleRules::Damage d = BattleRules::damage(*m_data, me, foe, move, crit, RandomUtil::range(0.0f, 1.0f), effectiveWeather());
            if (d.immune) { say("No afecta a " + who(foe, target) + "..."); return; }
            int amount = (std::max)(1, d.amount);
            if (amount >= foe.hp && foe.hp > 1) {
                if (const HeldItem* held = BattleRules::heldOf(*m_data, foe)) {
                    if (held->effect == HeldItem::Effect::FOCUS_BAND && chance(static_cast<float>(held->amount) / 100.0f)) { amount = foe.hp - 1; say(who(foe, target) + " aguantó con la Cinta focus."); }
                } else if (swindledWith(foe, HeldItem::Effect::FOCUS_BAND) && chance(static_cast<float>(m_data->held(foe.heldId)->amount) / 100.0f)) punishItem(target);
                if (amount >= foe.hp && foe.hp >= foe.maxHp && !foe.fainted()) {
                    if (const Ability* a = BattleRules::abilityOfKind(*m_data, foe, EffectSpec::Kind::SURVIVE, broken)) { amount = foe.hp - 1; say(foe.name + " resiste con " + a->name + "."); tag(target, 3, a->name); }
                }
            }
            amount = (std::min)(amount, foe.hp);
            foe.hp -= amount;
            dealt = amount;
            say(std::string(d.crit ? "¡Golpe crítico! " : "") + (d.effectiveness >= 4.0f ? "¡Es hipereficaz!" : d.effectiveness > 1.0f ? "¡Es supereficaz!" : d.effectiveness <= 0.25f ? "Es muy poco eficaz..." : d.effectiveness < 1.0f ? "No es muy eficaz..." : "Impacta."), slot, moveType, true, target);
            if (!foe.fainted()) {
                if (move.contact) fireTrigger(target, Ability::Trigger::ON_CONTACT, slot);
                fireTrigger(target, Ability::Trigger::ON_HIT, slot);
                if (d.crit) fireTrigger(target, Ability::Trigger::ON_CRIT_TAKEN, slot);
            }
            if (!me.fainted()) {
                if (const HeldItem* held = BattleRules::heldOf(*m_data, me)) {
                    if (held->effect == HeldItem::Effect::LIFE_ORB) { me.hp = (std::max)(0, me.hp - (std::max)(1, me.maxHp / BattleRules::RECOIL_ORB_DIVISOR)); say(who(me, slot) + " pierde PS por la Vidasfera."); }
                } else if (swindledWith(me, HeldItem::Effect::LIFE_ORB)) punishItem(slot);
            }
            if (foe.fainted()) {
                say(who(foe, target) + " se debilitó."); anim(target, 3); m_onField[target] = false;
                if (!me.fainted()) fireTrigger(slot, Ability::Trigger::ON_KO, target);
            }
        }
        if (move.effect.active() && me.status != Status::DESTABILIZED) applyEffect(move.effect, slot, target, dealt, &move); // un desestabilizado solo hace el daño del movimiento
    }

    // Cambia un nivel de estadística de ese hueco; 'source' = quien lo provoca (-1 = nadie / él mismo). Un hechizado recibe lo contrario; Coraza reflejo devuelve las bajadas; Oportunista copia las subidas.
    void changeStage(int tSlot, Stat stat, int amount, int source, bool reflected = false) {
        Fighter& t = cur(tSlot);
        if (t.status == Status::BEWITCHED) amount = -amount;
        const bool fromFoe = source >= 0 && sideOf(source) != sideOf(tSlot);
        if (amount < 0 && fromFoe && !reflected) {
            if (const Ability* a = BattleRules::abilityOfKind(*m_data, t, EffectSpec::Kind::REFLECT_DROPS, BattleRules::breaks(*m_data, cur(source)))) {
                say(t.name + " activa " + a->name);
                tag(tSlot, 3, a->name);
                changeStage(source, stat, amount, tSlot, true);
                return;
            }
        }
        int& stage = t.stage(stat);
        const int next = std::clamp(stage + amount, -BattleRules::MAX_STAGE, BattleRules::MAX_STAGE);
        if (next == stage) { say(std::string(who(t, tSlot)) + ": su " + EffectSpec::statName(stat) + " no puede " + (amount > 0 ? "subir" : "bajar") + " más."); return; }
        const int diff = (std::abs)(next - stage);
        stage = next;
        say(who(t, tSlot) + (amount > 0 ? " sube " : " baja ") + (diff > 1 ? "mucho " : "") + "su " + EffectSpec::statName(stat) + ".");
        tag(tSlot, amount > 0 ? 1 : 2, EffectSpec::statName(stat), static_cast<int>(stat), amount > 0 ? diff : -diff);
        if (amount > 0) { // Oportunista
            for (int other = 0; other < SLOTS; ++other) {
                if (sideOf(other) == sideOf(tSlot) || !inField(other) || !BattleRules::hasKind(*m_data, cur(other), EffectSpec::Kind::COPY_BOOSTS)) continue;
                int& mine = cur(other).stage(stat);
                const int copy = std::clamp(mine + diff, -BattleRules::MAX_STAGE, BattleRules::MAX_STAGE);
                if (copy == mine) continue;
                mine = copy;
                say(cur(other).name + " copia la mejora y sube su " + EffectSpec::statName(stat) + ".");
                tag(other, 1, EffectSpec::statName(stat), static_cast<int>(stat), diff);
            }
        }
        if (amount < 0 && fromFoe) fireTrigger(tSlot, Ability::Trigger::ON_STAT_DROP, -1);
    }

    // Intenta poner un estado: no si ya tiene uno o si su tipo es inmune a ese estado (según la tabla de tipos). 'say' = avisar si no se pone.
    void applyStatus(int tSlot, Status status, bool announce) {
        Fighter& t = cur(tSlot);
        if (status == Status::NONE) return;
        if (t.status != Status::NONE) { if (announce) say("Pero no tuvo efecto en " + who(t, tSlot) + "."); return; }
        if (!m_data->statusAffects(status, t.types)) { if (announce) say("No afecta a " + who(t, tSlot) + "..."); return; }
        t.status = status;
        t.statusTurns = BattleRules::isVolatile(status) ? RandomUtil::integer(BattleRules::VOLATILE_MIN_TURNS, BattleRules::VOLATILE_MAX_TURNS) : 0;
        say(who(t, tSlot) + " está " + EffectSpec::statusName(status) + ".");
    }

    // Aplica un efecto de 'user' (en 'target' si no es propio; en un movimiento que se dirige a cualquiera, siempre en el elegido). 'dealt' = daño hecho (retroceso, absorción).
    // 'sure' = la probabilidad ya se ha tirado.
    void applyEffect(const EffectSpec& e, int user, int target, int dealt, const Move* move, bool sure = false) {
        if (m_active[user] < 0 || m_active[target] < 0) return;
        Fighter& u = cur(user);
        const int tSlot = move && move->scope == Move::Scope::ANY ? target : e.self ? user : target;
        Fighter& t = cur(tSlot);
        if (t.fainted() && e.kind != EffectSpec::Kind::RECOIL) return;
        const bool secondary = e.kind == EffectSpec::Kind::STAT_STAGE || e.kind == EffectSpec::Kind::STATUS || e.kind == EffectSpec::Kind::FLINCH || e.kind == EffectSpec::Kind::CHIP;
        if (secondary && !sure && e.chance < 100.0f && !chance(e.chance / 100.0f)) return;
        switch (e.kind) {
            case EffectSpec::Kind::STAT_STAGE: changeStage(tSlot, e.stat, static_cast<int>(e.amount), user); break;
            case EffectSpec::Kind::STATUS: applyStatus(tSlot, e.status, e.chance >= 100.0f); break;
            case EffectSpec::Kind::HEAL_SELF: {
                const int gain = (std::min)(t.maxHp - t.hp, (std::max)(1, t.maxHp * static_cast<int>(e.amount) / 100));
                if (gain <= 0) { say("Pero no pasó nada."); break; }
                t.hp += gain;
                say(who(t, tSlot) + " recupera PS.");
                break;
            }
            case EffectSpec::Kind::RECOIL:
                if (dealt <= 0) break;
                u.hp = (std::max)(0, u.hp - (std::max)(1, dealt * static_cast<int>(e.amount) / 100));
                say(who(u, user) + " se hace daño por el retroceso.");
                break;
            case EffectSpec::Kind::DRAIN:
                if (dealt <= 0) break;
                u.hp = (std::min)(u.maxHp, u.hp + (std::max)(1, dealt * static_cast<int>(e.amount) / 100));
                say(who(u, user) + " absorbe energía.");
                break;
            case EffectSpec::Kind::FLINCH:
                if (!BattleRules::hasKind(*m_data, t, EffectSpec::Kind::FLINCH_IMMUNE)) t.flinched = true;
                break;
            case EffectSpec::Kind::CHIP:
                t.hp = (std::max)(0, t.hp - (std::max)(1, t.maxHp * static_cast<int>(e.amount) / 100));
                say(who(t, tSlot) + " se hiere al golpear.");
                break;
            default: break;
        }
    }

    // --- Fin de turno ---
    void endTurn() {
        const Weather weather = effectiveWeather();
        const int rock = BattleRules::typeByName(*m_data, "roca"), ground = BattleRules::typeByName(*m_data, "tierra"), steel = BattleRules::typeByName(*m_data, "acero");
        for (int slot = 0; slot < SLOTS; ++slot) {
            if (m_active[slot] < 0) continue;
            Fighter& f = cur(slot);
            if (f.fainted()) continue;
            if (f.status == Status::BURN || f.status == Status::POISON) {
                f.hp = (std::max)(0, f.hp - (std::max)(1, f.maxHp / (f.status == Status::BURN ? BattleRules::BURN_DIVISOR : BattleRules::POISON_DIVISOR)));
                say(who(f, slot) + " sufre por estar " + EffectSpec::statusName(f.status) + ".");
            }
            if (!f.fainted() && f.status == Status::INFESTED) { // infestado: pierde velocidad cada turno
                int& stage = f.stage(Stat::SPEED);
                if (stage > -BattleRules::MAX_STAGE) { --stage; say(who(f, slot) + " pierde velocidad por la infestación."); tag(slot, 2, EffectSpec::statName(Stat::SPEED), static_cast<int>(Stat::SPEED), -1); }
            }
            if (!f.fainted() && weather == Weather::SAND && !BattleRules::hasType(f, rock) && !BattleRules::hasType(f, ground) && !BattleRules::hasType(f, steel)) {
                f.hp = (std::max)(0, f.hp - (std::max)(1, f.maxHp / BattleRules::SAND_DIVISOR));
                say(who(f, slot) + " sufre la tormenta de arena.");
            }
            if (!f.fainted() && f.hp < f.maxHp) {
                if (const HeldItem* held = BattleRules::heldOf(*m_data, f)) {
                    if (held->effect == HeldItem::Effect::LEFTOVERS) {
                        f.hp = (std::min)(f.maxHp, f.hp + (std::max)(1, f.maxHp * held->amount / 100));
                        say(who(f, slot) + " recupera PS con los Restos.");
                    }
                } else if (swindledWith(f, HeldItem::Effect::LEFTOVERS)) punishItem(slot);
            }
            if (!f.fainted()) for (const Ability* a : BattleRules::abilitiesOf(*m_data, f)) {
                if (a->trigger != Ability::Trigger::END_TURN || !a->spec.active()) continue;
                const int target = opponentOf(slot);
                applyEffect(a->spec, slot, target >= 0 ? target : slot, 0, nullptr);
            }
            if (!f.fainted() && BattleRules::isVolatile(f.status) && --f.statusTurns <= 0) { say(who(f, slot) + " ya no está " + EffectSpec::statusName(f.status) + "."); f.status = Status::NONE; }
            if (f.fainted()) { say(who(f, slot) + " se debilitó."); anim(slot, 3); m_onField[slot] = false; }
        }
        if (m_weather != Weather::NONE && --m_weatherTurns <= 0) { say(std::string("Termina la ") + EffectSpec::weatherName(m_weather) + "."); m_weather = Weather::NONE; }
        m_coin = chance(0.5f);
        resolveFaints();
    }

    // Los pokémon del jugador que estuvieron frente al rival derrotado y siguen en pie se reparten su experiencia (según el nivel de cada uno).
    void awardXp(int foeSlot) {
        CRASH_SCOPE();
        const Fighter& foe = cur(foeSlot);
        m_defeated.push_back(foe.level);
        std::vector<int> winners;
        for (int i = 0; i < static_cast<int>(m_team[0].size()); ++i) if ((foe.faced >> i & 1u) && !m_team[0][static_cast<size_t>(i)].fainted()) winners.push_back(i);
        float rarity = 1.0f;
        if (const PokemonSpecies* species = m_data->speciesById(foe.speciesId)) rarity = m_data->xpMultiplier(*species);
        for (const int i : winners) m_team[0][static_cast<size_t>(i)].xpGain += ExperienceRules::pokemonXp(foe.level, m_team[0][static_cast<size_t>(i)].level, static_cast<int>(winners.size()), rarity);
    }

    // Quien cae: el rival saca al siguiente solo; si es el jugador, elige él (o pierde si no le queda ninguno).
    void resolveFaints() {
        CRASH_SCOPE();
        if (m_phase == Phase::OVER) return;
        for (const int slot : { 2, 3 }) {
            if (m_active[slot] < 0 || !cur(slot).fainted() || m_counted[slot]) continue;
            m_counted[slot] = true;
            awardXp(slot);
        }
        if (!anyAlive(1)) { finish(Outcome::WON); return; }
        for (const int slot : { 2, 3 }) {
            if (m_active[slot] < 0 || !cur(slot).fainted()) continue;
            m_counted[slot] = false;
            const int next = nextReserve(1);
            if (next < 0) { m_active[slot] = -1; m_onField[slot] = false; continue; }
            m_active[slot] = next;
            m_replaced[slot] = true;
            enter(slot);
        }
        if (!anyAlive(0)) { finish(Outcome::LOST); return; }
        for (const int slot : { 0, 1 }) {
            if (m_active[slot] < 0 || !cur(slot).fainted()) continue;
            if (nextReserve(0) < 0) { m_active[slot] = -1; m_onField[slot] = false; continue; } // sin relevo: el hueco queda vacío
            if (std::find(m_pending.begin(), m_pending.end(), slot) == m_pending.end()) m_pending.push_back(slot);
        }
        if (!m_pending.empty() && m_phase == Phase::CHOOSE) m_phase = Phase::FORCED_SWITCH;
    }

    void finish(Outcome outcome) {
        restoreForms();
        m_outcome = outcome;
        m_phase = Phase::OVER;
        say(outcome == Outcome::WON ? "¡Has ganado el combate!" : "Te has quedado sin pokémon...");
    }
};
