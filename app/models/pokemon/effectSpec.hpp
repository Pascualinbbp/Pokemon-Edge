#pragma once
#include <string>

// Efecto de combate (tabla effect, columnas de combate): lo comparten los movimientos y las habilidades. 'self' = recae en quien lo usa; si no, en el rival.
struct EffectSpec {
    enum class Kind { NONE, STAT_STAGE, STATUS, HEAL_SELF, RECOIL, DRAIN, PROTECT, CRIT_BOOST, FLINCH, ACCURACY_BOOST };
    enum class Stat { ATTACK, DEFENSE, SP_ATTACK, SP_DEFENSE, SPEED, ACCURACY, EVASION, COUNT };
    enum class Status { NONE, BURN, POISON, PARALYSIS };

    Kind kind = Kind::NONE;
    Stat stat = Stat::ATTACK;     // STAT_STAGE: la estadística
    Status status = Status::NONE; // STATUS: el estado
    bool self = false;
    float amount = 0.0f;          // niveles (STAT_STAGE, CRIT_BOOST), % (HEAL_SELF, RECOIL, DRAIN)
    float chance = 100.0f;        // % de que ocurra
    float multiplier = 1.0f;      // ACCURACY_BOOST

    bool active() const { return kind != Kind::NONE; }

    static Kind parseKind(const std::string& t) {
        return t == "STAT_STAGE" ? Kind::STAT_STAGE : t == "STATUS" ? Kind::STATUS : t == "HEAL_SELF" ? Kind::HEAL_SELF : t == "RECOIL" ? Kind::RECOIL : t == "DRAIN" ? Kind::DRAIN :
               t == "PROTECT" ? Kind::PROTECT : t == "CRIT_BOOST" ? Kind::CRIT_BOOST : t == "FLINCH" ? Kind::FLINCH : t == "ACCURACY_BOOST" ? Kind::ACCURACY_BOOST : Kind::NONE;
    }
    static Stat parseStat(const std::string& t) {
        return t == "DEFENSE" ? Stat::DEFENSE : t == "SP_ATTACK" ? Stat::SP_ATTACK : t == "SP_DEFENSE" ? Stat::SP_DEFENSE : t == "SPEED" ? Stat::SPEED : t == "ACCURACY" ? Stat::ACCURACY : t == "EVASION" ? Stat::EVASION : Stat::ATTACK;
    }
    static Status parseStatus(const std::string& t) { return t == "BURN" ? Status::BURN : t == "POISON" ? Status::POISON : t == "PARALYSIS" ? Status::PARALYSIS : Status::NONE; }
    static const char* statName(Stat s) {
        static constexpr const char* N[] = { "ataque", "defensa", "ataque especial", "defensa especial", "velocidad", "precisión", "evasión" };
        return N[static_cast<int>(s)];
    }
    static const char* statusName(Status s) { return s == Status::BURN ? "quemado" : s == Status::POISON ? "envenenado" : s == Status::PARALYSIS ? "paralizado" : ""; }
};
