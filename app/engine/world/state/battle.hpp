#pragma once
#include <deque>
#include <string>
#include <vector>
#include "../rules/battleRules.hpp"
#include "../rules/experienceRules.hpp"

// Combate por turnos 1 contra 1 con cambios de pokémon (única definición; no sabe nada de pantallas). El jugador manda una orden por turno
// (submit); el combate resuelve el turno entero y lo cuenta como una cola de mensajes, cada uno con una foto de los PS en ese momento, para
// que la interfaz los reproduzca uno a uno.
class Battle {
    public:
    using Fighter = BattleRules::Fighter;
    using Stat = EffectSpec::Stat;
    using Status = EffectSpec::Status;

    enum class Phase { CHOOSE, FORCED_SWITCH, OVER };
    enum class Outcome { NONE, WON, LOST };

    struct Action {
        enum class Kind { MOVE, SWITCH } kind = Kind::MOVE;
        int index = 0; // hueco del movimiento o posición en el equipo
    };

    // Un mensaje de la cola y cómo estaba la escena al darlo (lado 0 = jugador, 1 = rival).
    struct Message {
        std::string text;
        int speciesId[2] = { -1, -1 };
        std::string name[2];
        int level[2] = { 1, 1 };
        int hp[2] = { 0, 0 };
        int maxHp[2] = { 1, 1 };
        Status status[2] = { Status::NONE, Status::NONE };
        bool shiny[2] = { false, false };
        int attacker = -1;  // lado que ataca en este mensaje (-1 = ninguno)
        int moveType = -1;  // tipo del movimiento (para el efecto visual)
        bool hit = false;   // el golpe llegó (el defensor se sacude)
        int types[2][2] = { { -1, -1 }, { -1, -1 } };                       // tipos de cada pokémon en ese momento
        int stages[2][static_cast<int>(Stat::COUNT)] = {};                  // niveles de estadísticas de cada uno en ese momento
        int fxSide = -1;      // a quién le pasa algo visible (sube/baja una estadística, se activa una habilidad)
        int fxKind = 0;       // 0 nada, 1 sube, 2 baja, 3 habilidad
        int fxStat = -1;      // estadística (subir/bajar)
        int fxAmount = 0;     // niveles que cambia (con signo)
        std::string fxText;   // texto del aviso (nombre de la habilidad o de la estadística)
        bool onField[2] = { false, false }; // cada pokémon está ya en el campo (aún no ha salido de su pokéball o ya volvió a ella: no)
        int animKind = 0;     // 1 vuelve a su pokéball, 2 sale de su pokéball, 3 se debilita
        int animSide = -1;
    };

    Battle(const GameData& data, std::vector<Fighter> player, int playerActive, std::vector<Fighter> foe, std::string foeName)
        : m_data(&data), m_foeName(std::move(foeName)) {
        m_team[0] = std::move(player);
        m_team[1] = std::move(foe);
        m_active[0] = playerActive;
        m_active[1] = firstAlive(1);
        if (m_active[0] < 0 || m_active[0] >= static_cast<int>(m_team[0].size()) || m_team[0][m_active[0]].fainted()) m_active[0] = firstAlive(0);
        m_team[0][m_active[0]].participated = true;
        say(m_foeName + " quiere combatir");
        enter(1);
        enter(0);
        markFaced();
    }

    Phase phase() const { return m_phase; }
    Outcome outcome() const { return m_outcome; }
    const std::vector<Fighter>& team(int side) const { return m_team[side]; }
    int active(int side) const { return m_active[side]; }
    const Fighter& fighter(int side) const { return m_team[side][m_active[side]]; } // el que está en combate de ese lado
    const std::string& foeName() const { return m_foeName; }
    const std::vector<int>& defeatedLevels() const { return m_defeated; } // niveles de los pokémon rivales derrotados (el jugador gana experiencia por ellos)

    // Cola de mensajes pendientes de mostrar.
    bool hasMessage() const { return !m_messages.empty(); }
    const Message& front() const { return m_messages.front(); }
    void pop() { if (!m_messages.empty()) m_messages.pop_front(); }
    // Hasta que se vacía la cola no se aceptan órdenes.
    bool ready() const { return m_messages.empty() && m_phase != Phase::OVER; }

    // ¿Puede el jugador usar ese hueco (tiene movimiento y PP)? Sin ninguno usable, el hueco 0 es Forcejeo.
    bool canUse(int slot) const {
        const Fighter& f = cur(0);
        return slot >= 0 && slot < MoveRules::MAX_KNOWN && f.moves[slot] >= 0 && f.pp[slot] > 0;
    }
    bool mustStruggle() const { return !cur(0).usable(); }
    bool canSwitchTo(int index) const {
        return index >= 0 && index < static_cast<int>(m_team[0].size()) && index != m_active[0] && !m_team[0][index].fainted();
    }

    // Cambio obligatorio tras caer el pokémon activo.
    bool forceSwitch(int index) {
        if (m_phase != Phase::FORCED_SWITCH || !ready() || !canSwitchTo(index)) return false;
        switchTo(0, index);
        m_replaced[0] = true;
        m_phase = Phase::CHOOSE;
        return true;
    }

    // Manda la orden del jugador y resuelve el turno (el rival decide solo).
    bool submit(const Action& action) {
        if (m_phase != Phase::CHOOSE || !ready()) return false;
        if (action.kind == Action::Kind::MOVE && !mustStruggle() && !canUse(action.index)) return false;
        if (action.kind == Action::Kind::SWITCH && !canSwitchTo(action.index)) return false;
        const Action foeAction = chooseFoe();
        std::vector<Order> order = { { 0, action }, { 1, foeAction } };
        std::sort(order.begin(), order.end(), [&](const Order& a, const Order& b) { return before(a, b); });
        for (Fighter& f : m_team[0]) { f.protecting = false; }
        for (Fighter& f : m_team[1]) { f.protecting = false; }
        cur(0).flinched = cur(1).flinched = false;
        m_replaced[0] = m_replaced[1] = false;
        for (const Order& o : order) {
            if (m_phase == Phase::OVER) break;
            if (m_replaced[o.side]) continue; // el que entró a mitad de turno no actúa hasta el siguiente
            if (o.action.kind == Action::Kind::SWITCH) { if (!cur(o.side).fainted()) switchTo(o.side, o.action.index); continue; }
            if (cur(o.side).fainted() || cur(1 - o.side).fainted()) continue; // el que cae a mitad de turno no actúa
            act(o.side, o.action.index);
        }
        if (m_phase != Phase::OVER) endTurn();
        if (m_phase != Phase::OVER) afterFaints();
        return true;
    }

    // Copia de vuelta lo que cambió (PS, PP) para cada pokémon del jugador, por su posición de origen.
    const std::vector<Fighter>& playerTeam() const { return m_team[0]; }

    private:
    struct Order { int side; Action action; };

    const GameData* m_data;
    std::vector<Fighter> m_team[2];
    int m_active[2] = { 0, 0 };
    std::string m_foeName;
    std::deque<Message> m_messages;
    Phase m_phase = Phase::CHOOSE;
    Outcome m_outcome = Outcome::NONE;
    std::vector<int> m_defeated;
    bool m_onField[2] = { false, false };

    // --- Utilidades ---
    int firstAlive(int side) const {
        for (int i = 0; i < static_cast<int>(m_team[side].size()); ++i) if (!m_team[side][i].fainted()) return i;
        return -1;
    }
    Fighter& cur(int side) { return m_team[side][m_active[side]]; }
    const Fighter& cur(int side) const { return m_team[side][m_active[side]]; }
    static std::string who(const Fighter& f, int side) { return (side == 1 ? "El " : "") + f.name + (side == 1 ? " rival" : ""); }

    void say(const std::string& text, int attacker = -1, int moveType = -1, bool hit = false) {
        Message m;
        m.text = text;
        m.attacker = attacker;
        m.moveType = moveType;
        m.hit = hit;
        for (int side = 0; side < 2; ++side) {
            m.onField[side] = m_onField[side];
            const Fighter& f = m_team[side][m_active[side]];
            m.speciesId[side] = f.speciesId; m.name[side] = f.name; m.level[side] = f.level; m.hp[side] = f.hp; m.maxHp[side] = f.maxHp;
            m.status[side] = f.status; m.shiny[side] = f.shiny;
            for (int i = 0; i < 2; ++i) m.types[side][i] = i < static_cast<int>(f.types.size()) ? f.types[static_cast<size_t>(i)] : -1;
            for (int i = 0; i < static_cast<int>(Stat::COUNT); ++i) m.stages[side][i] = f.stages[static_cast<size_t>(i)];
        }
        m_messages.push_back(m);
    }

    // El último mensaje lleva una animación de pokéball (1 vuelve, 2 sale, 3 se debilita) para ese lado.
    void anim(int side, int kind) { m_messages.back().animKind = kind; m_messages.back().animSide = side; }

    // Marca el último mensaje con un aviso visual para 'side' (sube/baja una estadística, habilidad activada).
    void tag(int side, int kind, const std::string& text, int stat = -1, int amount = 0) {
        Message& m = m_messages.back();
        m.fxSide = side; m.fxKind = kind; m.fxText = text; m.fxStat = stat; m.fxAmount = amount;
    }

    bool chance(float p) const { return RandomUtil::range(0.0f, 1.0f) < p; }

    const Move* moveOf(const Fighter& f, int slot) const { return m_data->move(f.moves[slot]); }

    // Forcejeo: lo que usa quien se queda sin PP (sin tipo, con retroceso).
    const Move& struggle() const {
        static Move move = [] {
            Move m; m.id = -2; m.name = "Forcejeo"; m.typeId = -1; m.kind = MoveClass::Kind::PHYSICAL; m.power = BattleRules::STRUGGLE_POWER; m.contact = true;
            m.effect.kind = EffectSpec::Kind::RECOIL; m.effect.self = true; m.effect.amount = 25.0f;
            return m;
        }();
        return move;
    }

    // --- Entrada en combate y cambios ---
    // Habilidades "al entrar" del que acaba de salir.
    void enter(int side) {
        Fighter& f = cur(side);
        f.participated = side == 0 ? true : f.participated;
        m_onField[side] = true;
        say((side == 0 ? "¡Adelante, " + f.name + "!" : m_foeName + " envía a " + f.name + "."));
        anim(side, 2);
        for (const Ability* a : BattleRules::abilitiesOf(*m_data, f)) {
            if (a->trigger != Ability::Trigger::ON_ENTER || !a->spec.active()) continue;
            say(f.name + " activa " + a->name);
            tag(side, 3, a->name);
            applyEffect(a->spec, side, 1 - side, 0, nullptr);
        }
    }

    void switchTo(int side, int index) {
        Fighter& old = cur(side);
        if (!old.fainted()) { say(side == 0 ? "¡Vuelve, " + old.name + "!" : m_foeName + " retira a " + old.name + "."); anim(side, 1); }
        m_onField[side] = false;
        old.stages = {};
        old.flinched = false;
        old.protecting = false;
        m_active[side] = index;
        if (side == 0) cur(0).participated = true;
        enter(side);
        markFaced();
    }

    // --- IA del rival ---
    Action chooseFoe() {
        const Fighter& me = cur(1);
        const Fighter& you = cur(0);
        Action best; best.kind = Action::Kind::MOVE; best.index = 0;
        if (!me.usable()) return best;
        float bestScore = -1.0f;
        std::vector<int> options;
        for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) {
            const Move* move = moveOf(me, i);
            if (!move || me.pp[i] <= 0) continue;
            options.push_back(i);
            float score = 5.0f;
            if (move->damaging()) {
                const BattleRules::Damage d = BattleRules::damage(*m_data, me, you, *move, false, 0.5f);
                score = static_cast<float>(d.amount) * BattleRules::hitChance(*m_data, me, you, *move) + (d.amount >= you.hp ? 1000.0f : 0.0f);
            } else if (move->effect.kind == EffectSpec::Kind::HEAL_SELF) score = me.hpFraction() < 0.5f ? 80.0f : 0.0f;
            else if (move->effect.kind == EffectSpec::Kind::STATUS && you.status != Status::NONE) score = 0.0f;
            else if (move->effect.kind == EffectSpec::Kind::PROTECT) score = 2.0f;
            score *= RandomUtil::range(0.8f, 1.2f);
            if (score > bestScore) { bestScore = score; best.index = i; }
        }
        if (!options.empty() && chance(0.1f)) best.index = options[static_cast<size_t>(RandomUtil::integer(0, static_cast<int>(options.size()) - 1))]; // a veces improvisa
        return best;
    }

    // --- Orden de ataque ---
    int priorityOf(const Order& o) const {
        if (o.action.kind == Action::Kind::SWITCH) return 100;
        const Fighter& f = m_team[o.side][m_active[o.side]];
        const Move* move = f.usable() ? moveOf(f, o.action.index) : nullptr;
        return move ? move->priority : 0;
    }
    bool before(const Order& a, const Order& b) {
        const int pa = priorityOf(a), pb = priorityOf(b);
        if (pa != pb) return pa > pb;
        const float sa = BattleRules::speedOf(*m_data, m_team[a.side][m_active[a.side]]), sb = BattleRules::speedOf(*m_data, m_team[b.side][m_active[b.side]]);
        const bool claws[2] = { quickClaw(a.side), quickClaw(b.side) }; // Garra rápida: pasa primero dentro de su prioridad
        if (claws[0] != claws[1]) return a.side == 0 ? claws[0] : claws[1];
        if (sa != sb) return sa > sb;
        return a.side == 0 ? m_coin : !m_coin;
    }
    bool quickClaw(int side) const {
        const HeldItem* held = m_data->held(m_team[side][m_active[side]].heldId);
        if (!held || held->effect != HeldItem::Effect::QUICK_CLAW) return false;
        return chance(static_cast<float>(held->amount) / 100.0f);
    }
    bool m_coin = true;

    // --- Un movimiento ---
    void act(int side, int slot) {
        Fighter& me = cur(side);
        Fighter& foe = cur(1 - side);
        if (me.flinched) { say(who(me, side) + " se amedrentó y no pudo moverse."); me.flinched = false; return; }
        if (me.status == Status::PARALYSIS && chance(BattleRules::PARALYSIS_SKIP)) { say(who(me, side) + " está paralizado: ¡no se puede mover!"); return; }
        const bool struggling = !me.usable();
        const Move* chosen = struggling ? &struggle() : moveOf(me, slot);
        if (!chosen) return;
        const Move& move = *chosen;
        if (!struggling) --me.pp[slot];
        say(who(me, side) + " usó " + move.name + ".", side, move.typeId);

        if (move.effect.kind == EffectSpec::Kind::PROTECT) {
            const float p = std::pow(1.0f / 3.0f, static_cast<float>(me.protectStreak));
            if (chance(p)) { me.protecting = true; ++me.protectStreak; say(who(me, side) + " se protege."); }
            else { me.protectStreak = 0; say("¡Pero falló!"); }
            return;
        }
        me.protectStreak = 0;

        const bool targetsFoe = move.damaging() || !move.effect.self;
        if (targetsFoe && foe.protecting) { say(who(foe, 1 - side) + " se protegió."); return; }
        if (targetsFoe && !chance(BattleRules::hitChance(*m_data, me, foe, move))) { say("¡" + who(me, side) + " falló!"); return; }

        int dealt = 0;
        if (move.damaging()) {
            const bool crit = chance(BattleRules::critChance(BattleRules::critStages(*m_data, me, move)));
            const BattleRules::Damage d = BattleRules::damage(*m_data, me, foe, move, crit, RandomUtil::range(0.0f, 1.0f));
            if (d.immune) { say("No afecta a " + who(foe, 1 - side) + "..."); return; }
            int amount = d.amount;
            if (const HeldItem* held = m_data->held(foe.heldId)) {
                if (held->effect == HeldItem::Effect::FOCUS_BAND && amount >= foe.hp && foe.hp > 1 && chance(static_cast<float>(held->amount) / 100.0f)) {
                    amount = foe.hp - 1;
                    say(who(foe, 1 - side) + " aguantó con la Cinta focus.");
                }
            }
            amount = (std::min)(amount, foe.hp);
            foe.hp -= amount;
            dealt = amount;
            say(std::string(d.crit ? "¡Golpe crítico! " : "") + (d.effectiveness >= 4.0f ? "¡Es hipereficaz!" : d.effectiveness > 1.0f ? "¡Es supereficaz!" : d.effectiveness <= 0.25f ? "Es muy poco eficaz..." : d.effectiveness < 1.0f ? "No es muy eficaz..." : "Impacta."), side, move.typeId, true);
            if (move.contact && !foe.fainted()) {
                for (const Ability* a : BattleRules::abilitiesOf(*m_data, foe)) {
                    if (a->trigger != Ability::Trigger::ON_CONTACT || !a->spec.active()) continue;
                    say(foe.name + " activa " + a->name);
                    tag(1 - side, 3, a->name);
                    applyEffect(a->spec, 1 - side, side, 0, nullptr);
                }
            }
            if (const HeldItem* held = m_data->held(me.heldId)) {
                if (held->effect == HeldItem::Effect::LIFE_ORB && !me.fainted()) { me.hp = (std::max)(0, me.hp - (std::max)(1, me.maxHp / BattleRules::RECOIL_ORB_DIVISOR)); say(who(me, side) + " pierde PS por la Vidasfera."); }
            }
            if (foe.fainted()) { say(who(foe, 1 - side) + " se debilitó."); anim(1 - side, 3); m_onField[1 - side] = false; }
        }
        if (move.effect.active()) applyEffect(move.effect, side, targetsFoe && !move.effect.self ? 1 - side : side, dealt, &move);
        if (me.fainted()) { say(who(me, side) + " se debilitó."); anim(side, 3); m_onField[side] = false; }
        resolveFaints();
    }

    // Aplica un efecto de 'user' (en 'target' si no es propio). 'dealt' = daño hecho (retroceso, absorción).
    void applyEffect(const EffectSpec& e, int user, int target, int dealt, const Move*) {
        Fighter& u = cur(user);
        Fighter& t = cur(e.self ? user : target);
        const int tSide = e.self ? user : target;
        if (t.fainted() && e.kind != EffectSpec::Kind::RECOIL) return;
        const bool secondary = e.kind == EffectSpec::Kind::STAT_STAGE || e.kind == EffectSpec::Kind::STATUS || e.kind == EffectSpec::Kind::FLINCH;
        if (secondary && !chance(e.chance / 100.0f)) return;
        switch (e.kind) {
            case EffectSpec::Kind::STAT_STAGE: {
                int& stage = t.stage(e.stat);
                const int next = std::clamp(stage + static_cast<int>(e.amount), -BattleRules::MAX_STAGE, BattleRules::MAX_STAGE);
                if (next == stage) { say(std::string(who(t, tSide)) + ": su " + EffectSpec::statName(e.stat) + " no puede " + (e.amount > 0 ? "subir" : "bajar") + " más."); break; }
                const int diff = (std::abs)(next - stage);
                stage = next;
                say(who(t, tSide) + (e.amount > 0 ? " sube " : " baja ") + (diff > 1 ? "mucho " : "") + "su " + EffectSpec::statName(e.stat) + ".");
                tag(tSide, e.amount > 0 ? 1 : 2, EffectSpec::statName(e.stat), static_cast<int>(e.stat), e.amount > 0 ? diff : -diff);
                break;
            }
            case EffectSpec::Kind::STATUS:
                if (t.status != Status::NONE || e.status == Status::NONE) break;
                t.status = e.status;
                say(who(t, tSide) + " está " + EffectSpec::statusName(e.status) + ".");
                break;
            case EffectSpec::Kind::HEAL_SELF: {
                const int gain = (std::min)(t.maxHp - t.hp, (std::max)(1, t.maxHp * static_cast<int>(e.amount) / 100));
                if (gain <= 0) { say("Pero no pasó nada."); break; }
                t.hp += gain;
                say(who(t, tSide) + " recupera PS.");
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
        for (int side = 0; side < 2; ++side) {
            Fighter& f = cur(side);
            if (f.fainted()) continue;
            if (f.status == Status::BURN || f.status == Status::POISON) {
                f.hp = (std::max)(0, f.hp - (std::max)(1, f.maxHp / (f.status == Status::BURN ? BattleRules::BURN_DIVISOR : BattleRules::POISON_DIVISOR)));
                say(who(f, side) + " sufre por estar " + EffectSpec::statusName(f.status) + ".");
            }
            if (const HeldItem* held = m_data->held(f.heldId)) {
                if (held->effect == HeldItem::Effect::LEFTOVERS && !f.fainted() && f.hp < f.maxHp) {
                    f.hp = (std::min)(f.maxHp, f.hp + (std::max)(1, f.maxHp * held->amount / 100));
                    say(who(f, side) + " recupera PS con los Restos.");
                }
            }
            if (!f.fainted()) for (const Ability* a : BattleRules::abilitiesOf(*m_data, f)) {
                if (a->trigger != Ability::Trigger::END_TURN || !a->spec.active()) continue;
                applyEffect(a->spec, side, 1 - side, 0, nullptr);
            }
            if (f.fainted()) { say(who(f, side) + " se debilitó."); anim(side, 3); m_onField[side] = false; }
        }
        m_coin = chance(0.5f);
        resolveFaints();
    }

    // Los pokémon del jugador que estuvieron frente al rival derrotado y siguen en pie se reparten su experiencia (según el nivel de cada uno).
    void awardXp() {
        const Fighter& foe = cur(1);
        m_defeated.push_back(foe.level);
        std::vector<int> winners;
        for (int i = 0; i < static_cast<int>(m_team[0].size()); ++i) if ((foe.faced >> i & 1u) && !m_team[0][static_cast<size_t>(i)].fainted()) winners.push_back(i);
        float rarity = 1.0f;
        if (const PokemonSpecies* species = m_data->speciesById(foe.speciesId)) rarity = m_data->xpMultiplier(*species);
        for (const int i : winners) m_team[0][static_cast<size_t>(i)].xpGain += ExperienceRules::pokemonXp(foe.level, m_team[0][static_cast<size_t>(i)].level, static_cast<int>(winners.size()), rarity);
    }
    void markFaced() { cur(1).faced |= 1u << m_active[0]; }

    // Quien cae: el rival saca al siguiente solo; si es el jugador, elige él (o pierde si no le queda ninguno).
    void resolveFaints() {
        if (m_phase == Phase::OVER) return;
        if (cur(1).fainted() && !m_foeCounted) {
            m_foeCounted = true;
            awardXp();
        }
        if (cur(1).fainted()) {
            const int next = firstAlive(1);
            if (next < 0) { finish(Outcome::WON); return; }
            m_active[1] = next;
            m_foeCounted = false;
            m_replaced[1] = true;
            enter(1);
            markFaced();
        }
        if (cur(0).fainted() && m_phase == Phase::CHOOSE) {
            if (firstAlive(0) < 0) { finish(Outcome::LOST); return; }
            m_phase = Phase::FORCED_SWITCH;
        }
    }
    bool m_foeCounted = false;
    bool m_replaced[2] = { false, false };
    void afterFaints() { resolveFaints(); }

    void finish(Outcome outcome) {
        m_outcome = outcome;
        m_phase = Phase::OVER;
        say(outcome == Outcome::WON ? "¡Has ganado el combate!" : "Te has quedado sin pokémon...");
    }
};
