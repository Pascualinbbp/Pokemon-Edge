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
    inline constexpr float STAB = 1.5f;
    inline constexpr float BASE_CRIT[] = { 1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 3.0f, 1.0f / 2.0f }; // probabilidad según los niveles de crítico (0 = base)
    inline constexpr float PARALYSIS_SKIP = 0.25f;  // probabilidad de no poder moverse paralizado
    inline constexpr float PARALYSIS_SPEED = 0.5f;
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
        MoveRules::Slots moves = { -1, -1, -1, -1 };
        MoveRules::Slots pp = { 0, 0, 0, 0 };
        int abilityId = -1, passiveId = -1, heldId = -1;
        bool protecting = false;
        int protectStreak = 0;
        bool flinched = false;
        bool participated = false;
        unsigned faced = 0;   // (rival) bits de los pokémon del jugador que estuvieron en combate frente a él
        int xpGain = 0;       // (jugador) experiencia que ha ganado en este combate
        int source = -1; // posición en el equipo de origen

        bool fainted() const { return hp <= 0; }
        float hpFraction() const { return static_cast<float>(hp) / static_cast<float>((std::max)(1, maxHp)); }
        int& stage(Stat s) { return stages[static_cast<int>(s)]; }
        int stage(Stat s) const { return stages[static_cast<int>(s)]; }
        bool usable() const { for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) if (moves[i] >= 0 && pp[i] > 0) return true; return false; }
    };

    struct OwnedLike { // lo que hace falta de un pokémon guardado (así el combate no depende del almacén)
        int speciesId; int level; bool shiny; int hp; int heldId; MoveRules::Slots moves; MoveRules::Slots pp; std::array<int, BaseStats::COUNT> ivs; std::array<int, BaseStats::COUNT> evs; int natureId; int source;
    };

    // PS actuales de un pokémon guardado (hp < 0 = al máximo).
    inline Fighter makeFighter(const GameData& data, const OwnedLike& owned) {
        Fighter f;
        const PokemonSpecies* species = data.speciesById(owned.speciesId);
        f.speciesId = owned.speciesId;
        f.level = owned.level;
        f.shiny = owned.shiny;
        f.source = owned.source;
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
        f.abilityId = species->abilityId;
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

    inline float speedOf(const GameData&, const Fighter& f) {
        float value = static_cast<float>(f.speed) * stageMultiplier(f.stage(Stat::SPEED), false);
        if (f.status == Status::PARALYSIS) value *= PARALYSIS_SPEED;
        return value;
    }

    // Probabilidad de acertar (1 = no falla). Cuentan la precisión del movimiento, la del atacante, la evasión del defensor y las habilidades.
    inline float hitChance(const GameData& data, const Fighter& attacker, const Fighter& defender, const Move& move) {
        if (move.accuracy <= 0) return 1.0f;
        float chance = static_cast<float>(move.accuracy) / 100.0f;
        chance *= stageMultiplier(attacker.stage(Stat::ACCURACY) - defender.stage(Stat::EVASION), true);
        for (const Ability* a : abilitiesOf(data, attacker)) if (a->spec.kind == EffectSpec::Kind::ACCURACY_BOOST) chance *= a->spec.multiplier;
        return std::clamp(chance, 0.0f, 1.0f);
    }

    // Niveles de crítico del atacante con ese movimiento (objeto, habilidades y el propio movimiento).
    inline int critStages(const GameData& data, const Fighter& attacker, const Move& move) {
        int stages = 0;
        if (move.effect.kind == EffectSpec::Kind::CRIT_BOOST) stages += static_cast<int>(move.effect.amount);
        for (const Ability* a : abilitiesOf(data, attacker)) if (a->spec.kind == EffectSpec::Kind::CRIT_BOOST) stages += static_cast<int>(a->spec.amount);
        if (const HeldItem* held = data.held(attacker.heldId)) if (held->effect == HeldItem::Effect::RAZOR_CLAW) stages += 1;
        return stages;
    }
    inline float critChance(int stages) { return BASE_CRIT[std::clamp(stages, 0, 4)]; }

    struct Damage {
        int amount = 0;
        float effectiveness = 1.0f;
        bool crit = false;
        bool immune = false;
    };

    // Daño de un movimiento. 'crit' y 'roll' (0..1) los decide quien llama (así se puede probar con valores fijos).
    inline Damage damage(const GameData& data, const Fighter& attacker, const Fighter& defender, const Move& move, bool crit, float roll) {
        Damage result;
        result.effectiveness = data.typeChart.multiplier(move.typeId, defender.types);
        if (result.effectiveness <= TypeChart::IMMUNE) { result.immune = true; return result; }
        const bool special = move.kind == MoveClass::Kind::SPECIAL;
        const Stat atkStat = special ? Stat::SP_ATTACK : Stat::ATTACK, defStat = special ? Stat::SP_DEFENSE : Stat::DEFENSE;
        int atkStage = attacker.stage(atkStat), defStage = defender.stage(defStat);
        if (crit) { atkStage = (std::max)(atkStage, 0); defStage = (std::min)(defStage, 0); } // un crítico ignora lo que le estorba
        float attack = static_cast<float>(special ? attacker.spAttack : attacker.attack) * stageMultiplier(atkStage, false);
        const float defense = static_cast<float>(special ? defender.spDefense : defender.defense) * stageMultiplier(defStage, false);
        if (!special && attacker.status == Status::BURN) attack *= BURN_ATTACK;
        float value = ((2.0f * static_cast<float>(attacker.level) / 5.0f + 2.0f) * static_cast<float>(move.power) * attack / (std::max)(1.0f, defense)) / 50.0f + 2.0f;
        if (std::find(attacker.types.begin(), attacker.types.end(), move.typeId) != attacker.types.end()) value *= STAB;
        value *= result.effectiveness;
        if (crit) value *= CRIT_MULTIPLIER;
        value *= DAMAGE_MIN_ROLL + (1.0f - DAMAGE_MIN_ROLL) * std::clamp(roll, 0.0f, 1.0f);
        for (const Ability* a : abilitiesOf(data, attacker)) {
            if (a->effect == Ability::Effect::TYPE_BOOST && a->typeId == move.typeId && attacker.hpFraction() < a->hpBelow) value *= a->multiplier;
        }
        if (const HeldItem* held = data.held(attacker.heldId)) if (held->effect == HeldItem::Effect::LIFE_ORB) value *= 1.0f + static_cast<float>(held->amount) / 100.0f;
        result.crit = crit;
        result.amount = (std::max)(1, static_cast<int>(value));
        return result;
    }
}
