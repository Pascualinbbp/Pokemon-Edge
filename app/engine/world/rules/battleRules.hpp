#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"
#include "moveRules.hpp"
#include "pokemonRules.hpp"

// Fórmulas del combate por turnos (única definición): estadísticas, niveles de estadística, precisión, crítico, daño y orden de ataque.
namespace BattleRules {
    using Stat = EffectSpec::Stat;
    using Status = EffectSpec::Status;

    inline constexpr int MAX_STAGE = 6;
    inline constexpr float CRIT_MULTIPLIER = 1.5f;
    inline constexpr float STAB = 1.2f; // bonificación por atacar con un tipo propio (menor que en los juegos: casi todos los movimientos la tienen)
    inline constexpr float BASE_CRIT[] = { 1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 3.0f, 1.0f / 2.0f }; // probabilidad según los niveles de crítico (0 = base)
    inline constexpr float PARALYSIS_SKIP = 0.25f;  // probabilidad de no poder moverse paralizado
    inline constexpr float PARALYSIS_SPEED = 0.5f;
    inline constexpr float SOAKED_SPEED = 0.6f;
    inline constexpr float CRUSHED_DEFENSE = 0.67f;
    inline constexpr float INFESTED_SP_ATTACK = 0.67f;
    inline constexpr float FREEZE_THAW = 0.2f;       // probabilidad de descongelarse cada turno
    inline constexpr float CONFUSION_SELF_HIT = 0.33f;
    inline constexpr int CONFUSION_POWER = 40;
    inline constexpr int RECOIL_STATUS_DIVISOR = 8;  // aturdido y estafado: fracción de PS que pierde
    inline constexpr int SAND_DIVISOR = 16;
    inline constexpr float WEATHER_BOOST = 1.5f, WEATHER_CUT = 0.5f;
    inline constexpr float STANCE_BOOST = 1.3f, STANCE_CUT = 0.7f, HERO_BOOST = 1.5f;
    inline constexpr int VOLATILE_MIN_TURNS = 3, VOLATILE_MAX_TURNS = 5;
    inline constexpr int WEATHER_TURNS = 5;
    inline constexpr int TRANSFORM_PP = 5;
    using Weather = EffectSpec::Weather;
    // Estados que duran unos turnos y se quitan al cambiar de pokémon; los demás (quemado, envenenado, paralizado, congelado) duran todo el combate.
    inline bool isVolatile(Status s) { return s != Status::NONE && s != Status::BURN && s != Status::POISON && s != Status::PARALYSIS && s != Status::FROZEN; }
    inline constexpr float BURN_ATTACK = 0.5f;
    inline constexpr int BURN_DIVISOR = 16;         // fracción de PS que quita cada turno
    inline constexpr int POISON_DIVISOR = 8;
    inline constexpr int RECOIL_ORB_DIVISOR = 10;   // Vidasfera
    inline constexpr float DAMAGE_MIN_ROLL = 0.85f;
    inline constexpr int STRUGGLE_POWER = 50;

    // Un combatiente: copia de un pokémon con lo que cambia durante el combate.
    struct Fighter {
        int speciesId = -1;
        int level = 1;
        std::string name;
        bool shiny = false;
        std::vector<int> types;
        int maxHp = 1, hp = 1;
        int attack = 1, defense = 1, spAttack = 1, spDefense = 1, speed = 1;
        std::array<int, static_cast<int>(Stat::COUNT)> stages = {};
        Status status = Status::NONE;
        int statusTurns = 0;   // turnos que le quedan a un estado pasajero
        MoveRules::Slots moves = { -1, -1, -1, -1 };
        MoveRules::Slots pp = { 0, 0, 0, 0 };
        int lastMove = -1;     // hueco del último movimiento que usó (Oxidado)
        int abilityId = -1, passiveId = -1, heldId = -1;
        bool protecting = false;
        int protectStreak = 0;
        bool flinched = false;
        bool participated = false;
        bool illusion = false; // Ilusión sin romper
        bool stance = false;   // Cambio táctico: postura de ataque
        bool hero = false;     // Cambio heroico: ya salió del campo una vez
        bool transformed = false;
        MoveRules::Slots origMoves = { -1, -1, -1, -1 }, origPp = { 0, 0, 0, 0 };
        std::array<int, 5> origStats = {}; // ataque, ataque especial, defensa, defensa especial y velocidad de antes de transformarse
        unsigned faced = 0;   // (rival) bits de los pokémon del jugador que estuvieron en combate frente a él
        int xp = 0;           // (jugador) experiencia que llevaba hacia el siguiente nivel al empezar
        int xpGain = 0;       // (jugador) experiencia que ha ganado en este combate
        int source = -1; // posición en el equipo de origen

        bool fainted() const { return hp <= 0; }
        float hpFraction() const { return static_cast<float>(hp) / static_cast<float>((std::max)(1, maxHp)); }
        int& stage(Stat s) { return stages[static_cast<int>(s)]; }
        int stage(Stat s) const { return stages[static_cast<int>(s)]; }
        bool usable() const { for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) if (moves[i] >= 0 && pp[i] > 0) return true; return false; }
    };

    struct OwnedLike { // lo que hace falta de un pokémon guardado (así el combate no depende del almacén)
        int speciesId; int level; bool shiny; int hp; int heldId; MoveRules::Slots moves; MoveRules::Slots pp; std::array<int, BaseStats::COUNT> ivs; std::array<int, BaseStats::COUNT> evs; int natureId; int source; int abilityId = -1; int xp = 0;
    };

    // PS actuales de un pokémon guardado (hp < 0 = al máximo).
    inline Fighter makeFighter(const GameData& data, const OwnedLike& owned) {
        Fighter f;
        const PokemonSpecies* species = data.speciesById(owned.speciesId);
        f.speciesId = owned.speciesId;
        f.level = owned.level;
        f.shiny = owned.shiny;
        f.source = owned.source;
        f.xp = owned.xp;
        if (!species) return f;
        f.name = species->name;
        f.types = species->types;
        const Nature* nature = data.nature(owned.natureId);
        f.maxHp = PokemonRules::maxHp(*species, owned.level, owned.ivs[0], owned.evs[0]);
        f.hp = owned.hp < 0 ? f.maxHp : (std::min)(owned.hp, f.maxHp);
        f.attack = StatRules::of(species->stats, 1, owned.level, owned.ivs[1], owned.evs[1], nature);
        f.spAttack = StatRules::of(species->stats, 2, owned.level, owned.ivs[2], owned.evs[2], nature);
        f.defense = StatRules::of(species->stats, 3, owned.level, owned.ivs[3], owned.evs[3], nature);
        f.spDefense = StatRules::of(species->stats, 4, owned.level, owned.ivs[4], owned.evs[4], nature);
        f.speed = StatRules::of(species->stats, 5, owned.level, owned.ivs[5], owned.evs[5], nature);
        f.abilityId = data.abilityFor(*species, owned.abilityId);
        f.passiveId = species->passiveId;
        f.heldId = owned.heldId;
        f.moves = owned.moves;
        for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) {
            const Move* move = data.move(owned.moves[i]);
            f.pp[i] = !move ? 0 : owned.pp[i] < 0 ? move->pp : (std::min)(owned.pp[i], move->pp);
        }
        return f;
    }

    // Multiplicador de un nivel de estadística: +1 = x1,5; +2 = x2... -1 = x2/3... (precisión y evasión: x4/3, x5/3...).
    inline float stageMultiplier(int stage, bool accuracy) {
        const int s = std::clamp(stage, -MAX_STAGE, MAX_STAGE);
        const float base = accuracy ? 3.0f : 2.0f;
        return s >= 0 ? (base + static_cast<float>(s)) / base : base / (base - static_cast<float>(s));
    }

    // Las habilidades del combatiente (activa y pasiva), de mayor a menor prioridad.
    inline std::vector<const Ability*> abilitiesOf(const GameData& data, const Fighter& f) {
        std::vector<const Ability*> list;
        for (const int id : { f.abilityId, f.passiveId }) if (const Ability* a = id >= 0 ? data.ability(id) : nullptr) list.push_back(a);
        std::stable_sort(list.begin(), list.end(), [](const Ability* a, const Ability* b) { return a->priority > b->priority; });
        return list;
    }
    // Su habilidad de ese efecto (nullptr si no tiene). 'broken' = quien la mira es un Rompemoldes y la ignora.
    inline const Ability* abilityOfKind(const GameData& data, const Fighter& f, EffectSpec::Kind kind, bool broken = false) {
        if (broken) return nullptr;
        for (const Ability* a : abilitiesOf(data, f)) if (a->spec.kind == kind) return a;
        return nullptr;
    }
    inline bool hasKind(const GameData& data, const Fighter& f, EffectSpec::Kind kind) { return abilityOfKind(data, f, kind) != nullptr; }
    // ¿Ignora el atacante las habilidades del defensor?
    inline bool breaks(const GameData& data, const Fighter& attacker) { return hasKind(data, attacker, EffectSpec::Kind::MOLD_BREAKER); }

    // Objeto que cuenta de verdad: no el de un pokémon estafado ni el de uno con Indefenso.
    inline const HeldItem* heldOf(const GameData& data, const Fighter& f) {
        if (f.status == Status::SWINDLED || hasKind(data, f, EffectSpec::Kind::KLUTZ)) return nullptr;
        return data.held(f.heldId);
    }

    inline int typeByName(const GameData& data, const char* name) {
        for (const PokemonType& t : data.types) if (t.name == name) return t.id;
        return -1;
    }
    inline bool hasType(const Fighter& f, int type) { return std::find(f.types.begin(), f.types.end(), type) != f.types.end(); }

    inline bool condMet(const EffectSpec& e, const Fighter& f, Weather weather) {
        switch (e.cond) {
            case EffectSpec::Cond::RAIN: return weather == Weather::RAIN;
            case EffectSpec::Cond::SUN: return weather == Weather::SUN;
            case EffectSpec::Cond::SAND: return weather == Weather::SAND;
            case EffectSpec::Cond::SNOW: return weather == Weather::SNOW;
            case EffectSpec::Cond::STATUS: return f.status != Status::NONE;
            case EffectSpec::Cond::FULL_HP: return f.hp >= f.maxHp;
            default: return true;
        }
    }

    // Factor que sus habilidades (y su postura) ponen a una estadística.
    inline float statFactor(const GameData& data, const Fighter& f, Stat stat, Weather weather) {
        float factor = 1.0f;
        for (const Ability* a : abilitiesOf(data, f)) if (a->spec.kind == EffectSpec::Kind::STAT_MULT && a->spec.stat == stat && condMet(a->spec, f, weather)) factor *= a->spec.multiplier;
        if (hasKind(data, f, EffectSpec::Kind::STANCE)) {
            const bool offense = stat == Stat::ATTACK || stat == Stat::SP_ATTACK, defense = stat == Stat::DEFENSE || stat == Stat::SP_DEFENSE;
            if (f.stance) factor *= offense ? STANCE_BOOST : defense ? STANCE_CUT : 1.0f;
            else factor *= offense ? STANCE_CUT : defense ? STANCE_BOOST : 1.0f;
        }
        if (f.hero && hasKind(data, f, EffectSpec::Kind::HERO) && (stat == Stat::ATTACK || stat == Stat::SP_ATTACK || stat == Stat::SPEED)) factor *= HERO_BOOST;
        return factor;
    }

    inline float speedOf(const GameData& data, const Fighter& f, Weather weather = Weather::NONE) {
        float value = static_cast<float>(f.speed) * stageMultiplier(f.stage(Stat::SPEED), false) * statFactor(data, f, Stat::SPEED, weather);
        if (f.status == Status::PARALYSIS) value *= PARALYSIS_SPEED;
        if (f.status == Status::SOAKED) value *= SOAKED_SPEED;
        return value;
    }

    // Probabilidad de acertar (1 = no falla). Cuentan la precisión del movimiento, la del atacante, la evasión del defensor (que un enraizado no tiene) y las habilidades.
    inline float hitChance(const GameData& data, const Fighter& attacker, const Fighter& defender, const Move& move, Weather weather = Weather::NONE) {
        if (move.accuracy <= 0) return 1.0f;
        const bool broken = breaks(data, attacker);
        float chance = static_cast<float>(move.accuracy) / 100.0f;
        const bool ignoreEvasion = defender.status == Status::ROOTED || hasKind(data, attacker, EffectSpec::Kind::IGNORE_STAGES);
        const bool ignoreAccuracy = hasKind(data, defender, EffectSpec::Kind::IGNORE_STAGES) && !broken;
        chance *= stageMultiplier((ignoreAccuracy ? 0 : attacker.stage(Stat::ACCURACY)) - (ignoreEvasion ? 0 : defender.stage(Stat::EVASION)), true);
        for (const Ability* a : abilitiesOf(data, attacker)) if (a->spec.kind == EffectSpec::Kind::ACCURACY_BOOST) chance *= a->spec.multiplier;
        if (!broken && defender.status != Status::ROOTED) for (const Ability* a : abilitiesOf(data, defender)) if (a->spec.kind == EffectSpec::Kind::EVASION_BOOST && condMet(a->spec, defender, weather)) chance /= (std::max)(0.1f, a->spec.multiplier);
        return std::clamp(chance, 0.0f, 1.0f);
    }

    // Niveles de crítico del atacante con ese movimiento (objeto, habilidades y el propio movimiento).
    inline int critStages(const GameData& data, const Fighter& attacker, const Move& move) {
        int stages = 0;
        if (move.effect.kind == EffectSpec::Kind::CRIT_BOOST) stages += static_cast<int>(move.effect.amount);
        for (const Ability* a : abilitiesOf(data, attacker)) if (a->spec.kind == EffectSpec::Kind::CRIT_BOOST) stages += static_cast<int>(a->spec.amount);
        if (const HeldItem* held = heldOf(data, attacker)) if (held->effect == HeldItem::Effect::RAZOR_CLAW) stages += 1;
        return stages;
    }
    inline float critChance(int stages) { return BASE_CRIT[std::clamp(stages, 0, 4)]; }
    // ¿Es crítico? Un indignado no los hace nunca; contra un agrietado todos los golpes lo son. 'roll' (0..1) es la tirada.
    inline bool isCrit(const GameData& data, const Fighter& attacker, const Fighter& defender, const Move& move, float roll) {
        if (attacker.status == Status::INDIGNANT) return false;
        if (defender.status == Status::CRACKED) return true;
        return roll < critChance(critStages(data, attacker, move));
    }

    // Tipo con el que ataca de verdad un movimiento (Normalidad lo vuelve normal).
    inline int moveTypeOf(const GameData& data, const Fighter& attacker, const Move& move) {
        if (move.typeId > 0 && hasKind(data, attacker, EffectSpec::Kind::NORMALIZE)) { const int normal = typeByName(data, "normal"); if (normal > 0) return normal; }
        return move.typeId;
    }

    struct Damage {
        int amount = 0;
        float effectiveness = 1.0f;
        bool crit = false;
        bool immune = false;
    };

    // Daño de un movimiento. 'crit' y 'roll' (0..1) los decide quien llama (así se puede probar con valores fijos).
    inline Damage damage(const GameData& data, const Fighter& attacker, const Fighter& defender, const Move& move, bool crit, float roll, Weather weather = Weather::NONE) {
        Damage result;
        const int type = moveTypeOf(data, attacker, move);
        const bool broken = breaks(data, attacker);
        result.effectiveness = data.typeChart.multiplier(type, defender.types);
        if (result.effectiveness <= TypeChart::IMMUNE) { result.immune = true; return result; }
        bool special = move.kind == MoveClass::Kind::SPECIAL;
        if (attacker.status == Status::POSSESSED) special = !special; // poseído: el movimiento se calcula con la otra estadística
        const Stat atkStat = special ? Stat::SP_ATTACK : Stat::ATTACK, defStat = special ? Stat::SP_DEFENSE : Stat::DEFENSE;
        int atkStage = attacker.stage(atkStat), defStage = defender.stage(defStat);
        if (hasKind(data, defender, EffectSpec::Kind::IGNORE_STAGES) && !broken) atkStage = 0;
        if (hasKind(data, attacker, EffectSpec::Kind::IGNORE_STAGES)) defStage = 0;
        if (crit) { atkStage = (std::max)(atkStage, 0); defStage = (std::min)(defStage, 0); } // un crítico ignora lo que le estorba
        float attack = static_cast<float>(special ? attacker.spAttack : attacker.attack) * stageMultiplier(atkStage, false) * statFactor(data, attacker, atkStat, weather);
        float defense = static_cast<float>(special ? defender.spDefense : defender.defense) * stageMultiplier(defStage, false) * statFactor(data, defender, defStat, weather);
        if (!special && attacker.status == Status::BURN) attack *= BURN_ATTACK;
        if (special && attacker.status == Status::INFESTED) attack *= INFESTED_SP_ATTACK;
        if (defender.status == Status::CRUSHED) defense *= CRUSHED_DEFENSE;
        float value = ((2.0f * static_cast<float>(attacker.level) / 5.0f + 2.0f) * static_cast<float>(move.power) * attack / (std::max)(1.0f, defense)) / 50.0f + 2.0f;
        if (attacker.status != Status::INDIGNANT && hasType(attacker, type)) {
            float stab = STAB;
            if (const Ability* a = abilityOfKind(data, attacker, EffectSpec::Kind::STAB_BOOST)) stab = (std::max)(stab, a->spec.multiplier);
            value *= stab;
        }
        value *= result.effectiveness;
        if (crit) { float factor = CRIT_MULTIPLIER; if (const Ability* a = abilityOfKind(data, attacker, EffectSpec::Kind::CRIT_DAMAGE)) factor = a->spec.multiplier; value *= factor; }
        value *= DAMAGE_MIN_ROLL + (1.0f - DAMAGE_MIN_ROLL) * std::clamp(roll, 0.0f, 1.0f);
        for (const Ability* a : abilitiesOf(data, attacker)) {
            if (a->effect == Ability::Effect::TYPE_BOOST && a->typeId == type && attacker.hpFraction() < a->hpBelow) value *= a->multiplier;
            if (a->spec.kind == EffectSpec::Kind::TAG_BOOST) for (const MoveTag& tag : data.moveTags) if (tag.name == a->spec.param && std::find(move.tags.begin(), move.tags.end(), tag.id) != move.tags.end()) value *= a->spec.multiplier;
            if (a->spec.kind == EffectSpec::Kind::WEAK_BOOST && static_cast<float>(move.power) <= a->spec.amount) value *= a->spec.multiplier;
        }
        for (const Ability* a : abilitiesOf(data, attacker)) { // Bioluminiscencia: más daño con los movimientos de luz (etiqueta "Luz")
            if (a->effect != Ability::Effect::BIOLUMINESCENCE) continue;
            for (const MoveTag& tag : data.moveTags) if (tag.name == "Luz" && std::find(move.tags.begin(), move.tags.end(), tag.id) != move.tags.end()) value *= a->multiplier;
        }
        if (!broken) for (const Ability* a : abilitiesOf(data, defender)) {
            if (a->spec.kind == EffectSpec::Kind::RESIST_SUPER && result.effectiveness > 1.0f) value *= a->spec.multiplier;
            if (a->spec.kind == EffectSpec::Kind::RESIST_FULL_HP && defender.hp >= defender.maxHp) value *= a->spec.multiplier;
        }
        if (weather == Weather::RAIN || weather == Weather::SUN) {
            const int water = typeByName(data, "agua"), fire = typeByName(data, "fuego");
            const bool rain = weather == Weather::RAIN;
            if (type == water) value *= rain ? WEATHER_BOOST : WEATHER_CUT;
            else if (type == fire) value *= rain ? WEATHER_CUT : WEATHER_BOOST;
        }
        if (const HeldItem* held = heldOf(data, attacker)) if (held->effect == HeldItem::Effect::LIFE_ORB) value *= 1.0f + static_cast<float>(held->amount) / 100.0f;
        result.crit = crit;
        result.amount = (std::max)(1, static_cast<int>(value));
        return result;
    }
}
