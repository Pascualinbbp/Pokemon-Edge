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
    bool canUse(int slot, int move) const {
        if (!inField(slot)) return false;
        const Fighter& f = fighter(slot);
        return move >= 0 && move < MoveRules::MAX_KNOWN && f.moves[static_cast<size_t>(move)] >= 0 && f.pp[static_cast<size_t>(move)] > 0;
    }
    bool mustStruggle(int slot) const { return inField(slot) && !fighter(slot).usable(); }
    // ¿Hay que elegir a quién ataca ese movimiento? (solo en dobles, si ataca al rival y quedan dos rivales en pie)
    bool needsTarget(int slot, int move) const {
        if (!m_doubles || !inField(slot)) return false;
        const Move* m = mustStruggle(slot) ? nullptr : moveOf(fighter(slot), move);
        if (m && m->scope == Move::Scope::ANY) return true; // aliado, uno mismo o rival
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
                if (!canSwitchTo(a.index, reserved)) return false;
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
    void triggerEntry(int slot) {
        if (!inField(slot)) return;
        Fighter& f = cur(slot);
        for (const Ability* a : BattleRules::abilitiesOf(*m_data, f)) {
            if (a->trigger != Ability::Trigger::ON_ENTER || !a->spec.active()) continue;
            say(f.name + " activa " + a->name);
            tag(slot, 3, a->name);
            if (a->spec.self) { applyEffect(a->spec, slot, slot, 0, nullptr); continue; }
            for (int other = 0; other < SLOTS; ++other) if (sideOf(other) != sideOf(slot) && inField(other)) applyEffect(a->spec, slot, other, 0, nullptr);
        }
    }

    void switchTo(int slot, int index) {
        if (m_active[slot] >= 0) {
            Fighter& old = cur(slot);
            if (!old.fainted()) { say(sideOf(slot) == 0 ? "¡Vuelve, " + old.name + "!" : m_foeName + " retira a " + old.name + "."); anim(slot, 1); }
            old.stages = {};
            old.flinched = false;
            old.protecting = false;
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
            if (!move || me.pp[static_cast<size_t>(i)] <= 0) continue;
            for (const int t : targets) {
                const Fighter& you = cur(t);
                float score = 5.0f;
                if (move->damaging()) {
                    const BattleRules::Damage d = BattleRules::damage(*m_data, me, you, *move, false, 0.5f);
                    score = static_cast<float>(d.amount) * BattleRules::hitChance(*m_data, me, you, *move) + (d.amount >= you.hp ? 1000.0f : 0.0f);
                } else if (move->effect.kind == EffectSpec::Kind::HEAL_SELF) score = me.hpFraction() < 0.5f ? 80.0f : 0.0f;
                else if (move->effect.kind == EffectSpec::Kind::STATUS && you.status != Status::NONE) score = 0.0f;
                else if (move->effect.kind == EffectSpec::Kind::PROTECT) score = 2.0f;
                score *= RandomUtil::range(0.8f, 1.2f);
                Action a; a.index = i; a.target = t;
                options.push_back(a);
                if (score > bestScore) { bestScore = score; best = a; }
            }
        }
        if (!options.empty() && chance(0.1f)) best = options[static_cast<size_t>(RandomUtil::integer(0, static_cast<int>(options.size()) - 1))]; // a veces improvisa
        return best;
    }

    // --- Orden de ataque ---
    int priorityOf(const Order& o) const {
        if (o.action.kind == Action::Kind::SWITCH) return 100;
        const Fighter& f = cur(o.slot);
        const Move* move = f.usable() ? moveOf(f, o.action.index) : nullptr;
        return move ? move->priority : 0;
    }
    bool before(const Order& a, const Order& b) {
        const int pa = priorityOf(a), pb = priorityOf(b);
        if (pa != pb) return pa > pb;
        const float sa = BattleRules::speedOf(*m_data, cur(a.slot)), sb = BattleRules::speedOf(*m_data, cur(b.slot));
        if (a.claw != b.claw) return a.claw; // Garra rápida: pasa primero dentro de su prioridad
        if (sa != sb) return sa > sb;
        return a.slot < b.slot ? m_coin : !m_coin;
    }
    bool quickClaw(int slot) const {
        const HeldItem* held = m_data->held(cur(slot).heldId);
        if (!held || held->effect != HeldItem::Effect::QUICK_CLAW) return false;
        return chance(static_cast<float>(held->amount) / 100.0f);
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
    void act(int slot, int moveSlot, int targetSlot) {
        CRASH_SCOPE();
        Fighter& me = cur(slot);
        if (me.flinched) { say(who(me, slot) + " se amedrentó y no pudo moverse."); me.flinched = false; return; }
        if (me.status == Status::PARALYSIS && chance(BattleRules::PARALYSIS_SKIP)) { say(who(me, slot) + " está paralizado: ¡no se puede mover!"); return; }
        const bool struggling = !me.usable();
        const Move* chosen = struggling ? &struggle() : moveOf(me, moveSlot);
        if (!chosen) return;
        const Move& move = *chosen;
        if (!struggling) --me.pp[static_cast<size_t>(moveSlot)];
        const bool targetsFoe = targetsFoeOf(move) || move.scope == Move::Scope::ANY;
        const std::vector<int> targets = targetsOf(slot, move, targetSlot);
        say(who(me, slot) + " usó " + move.name + ".", slot, move.typeId, false, targetsFoe ? targets.front() : slot);

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
        if (me.fainted() && m_onField[slot]) { say(who(me, slot) + " se debilitó."); anim(slot, 3); m_onField[slot] = false; }
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
        Fighter& foe = cur(target);
        const bool hostile = targetsFoe && target != slot; // cualquier otro pokémon del campo, aliado incluido (Terremoto): la protección lo frena
        if (hostile && foe.protecting) { say(who(foe, target) + " se protegió."); return; }
        if (hostile && !chance(BattleRules::hitChance(*m_data, me, foe, move))) { say("¡" + who(me, slot) + " falló!"); return; }

        int dealt = 0;
        if (move.damaging()) {
            const bool crit = chance(BattleRules::critChance(BattleRules::critStages(*m_data, me, move)));
            const BattleRules::Damage d = BattleRules::damage(*m_data, me, foe, move, crit, RandomUtil::range(0.0f, 1.0f));
            if (d.immune) { say("No afecta a " + who(foe, target) + "..."); return; }
            int amount = (std::max)(1, d.amount);
            if (const HeldItem* held = m_data->held(foe.heldId)) {
                if (held->effect == HeldItem::Effect::FOCUS_BAND && amount >= foe.hp && foe.hp > 1 && chance(static_cast<float>(held->amount) / 100.0f)) {
                    amount = foe.hp - 1;
                    say(who(foe, target) + " aguantó con la Cinta focus.");
                }
            }
            amount = (std::min)(amount, foe.hp);
            foe.hp -= amount;
            dealt = amount;
            say(std::string(d.crit ? "¡Golpe crítico! " : "") + (d.effectiveness >= 4.0f ? "¡Es hipereficaz!" : d.effectiveness > 1.0f ? "¡Es supereficaz!" : d.effectiveness <= 0.25f ? "Es muy poco eficaz..." : d.effectiveness < 1.0f ? "No es muy eficaz..." : "Impacta."), slot, move.typeId, true, target);
            if (move.contact && !foe.fainted()) {
                for (const Ability* a : BattleRules::abilitiesOf(*m_data, foe)) {
                    if (a->trigger != Ability::Trigger::ON_CONTACT || !a->spec.active()) continue;
                    say(foe.name + " activa " + a->name);
                    tag(target, 3, a->name);
                    applyEffect(a->spec, target, slot, 0, nullptr);
                }
            }
            if (const HeldItem* held = m_data->held(me.heldId)) {
                if (held->effect == HeldItem::Effect::LIFE_ORB && !me.fainted()) { me.hp = (std::max)(0, me.hp - (std::max)(1, me.maxHp / BattleRules::RECOIL_ORB_DIVISOR)); say(who(me, slot) + " pierde PS por la Vidasfera."); }
            }
            if (foe.fainted()) { say(who(foe, target) + " se debilitó."); anim(target, 3); m_onField[target] = false; }
        }
        if (move.effect.active()) applyEffect(move.effect, slot, target, dealt, &move);
    }

    // Aplica un efecto de 'user' (en 'target' si no es propio). 'dealt' = daño hecho (retroceso, absorción).
    void applyEffect(const EffectSpec& e, int user, int target, int dealt, const Move*) {
        if (m_active[user] < 0 || m_active[target] < 0) return;
        Fighter& u = cur(user);
        const int tSlot = e.self ? user : target;
        Fighter& t = cur(tSlot);
        if (t.fainted() && e.kind != EffectSpec::Kind::RECOIL) return;
        const bool secondary = e.kind == EffectSpec::Kind::STAT_STAGE || e.kind == EffectSpec::Kind::STATUS || e.kind == EffectSpec::Kind::FLINCH;
        if (secondary && !chance(e.chance / 100.0f)) return;
        switch (e.kind) {
            case EffectSpec::Kind::STAT_STAGE: {
                int& stage = t.stage(e.stat);
                const int next = std::clamp(stage + static_cast<int>(e.amount), -BattleRules::MAX_STAGE, BattleRules::MAX_STAGE);
                if (next == stage) { say(std::string(who(t, tSlot)) + ": su " + EffectSpec::statName(e.stat) + " no puede " + (e.amount > 0 ? "subir" : "bajar") + " más."); break; }
                const int diff = (std::abs)(next - stage);
                stage = next;
                say(who(t, tSlot) + (e.amount > 0 ? " sube " : " baja ") + (diff > 1 ? "mucho " : "") + "su " + EffectSpec::statName(e.stat) + ".");
                tag(tSlot, e.amount > 0 ? 1 : 2, EffectSpec::statName(e.stat), static_cast<int>(e.stat), e.amount > 0 ? diff : -diff);
                break;
            }
            case EffectSpec::Kind::STATUS:
                if (t.status != Status::NONE || e.status == Status::NONE) break;
                t.status = e.status;
                say(who(t, tSlot) + " está " + EffectSpec::statusName(e.status) + ".");
                break;
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
                t.flinched = true;
                break;
            default: break;
        }
    }

    // --- Fin de turno ---
    void endTurn() {
        for (int slot = 0; slot < SLOTS; ++slot) {
            if (m_active[slot] < 0) continue;
            Fighter& f = cur(slot);
            if (f.fainted()) continue;
            if (f.status == Status::BURN || f.status == Status::POISON) {
                f.hp = (std::max)(0, f.hp - (std::max)(1, f.maxHp / (f.status == Status::BURN ? BattleRules::BURN_DIVISOR : BattleRules::POISON_DIVISOR)));
                say(who(f, slot) + " sufre por estar " + EffectSpec::statusName(f.status) + ".");
            }
            if (const HeldItem* held = m_data->held(f.heldId)) {
                if (held->effect == HeldItem::Effect::LEFTOVERS && !f.fainted() && f.hp < f.maxHp) {
                    f.hp = (std::min)(f.maxHp, f.hp + (std::max)(1, f.maxHp * held->amount / 100));
                    say(who(f, slot) + " recupera PS con los Restos.");
                }
            }
            if (!f.fainted()) for (const Ability* a : BattleRules::abilitiesOf(*m_data, f)) {
                if (a->trigger != Ability::Trigger::END_TURN || !a->spec.active()) continue;
                const int target = opponentOf(slot);
                applyEffect(a->spec, slot, target >= 0 ? target : slot, 0, nullptr);
            }
            if (f.fainted()) { say(who(f, slot) + " se debilitó."); anim(slot, 3); m_onField[slot] = false; }
        }
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
        m_outcome = outcome;
        m_phase = Phase::OVER;
        say(outcome == Outcome::WON ? "¡Has ganado el combate!" : "Te has quedado sin pokémon...");
    }
};
