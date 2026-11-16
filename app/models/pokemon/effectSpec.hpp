#pragma once
#include <string>

// Efecto de combate (tabla effect, columnas de combate): lo comparten los movimientos, las habilidades y los estados. 'self' = recae en quien lo usa; si no, en el rival.
// La fila de effect es genérica (qué hace); la probabilidad la pone quien lo usa (move.effect_chance, ability.effect_chance).
struct EffectSpec {
    enum class Kind {
        NONE, STAT_STAGE, STATUS, HEAL_SELF, RECOIL, DRAIN, PROTECT, CRIT_BOOST, FLINCH, ACCURACY_BOOST,
        WEATHER,          // fija el clima (param)
        STAT_MULT,        // multiplica una estadística (param) por 'multiplier', si se cumple la condición
        EVASION_BOOST,    // evasión: los ataques rivales aciertan 1/multiplier (con la condición)
        TAG_BOOST,        // potencia los movimientos con la etiqueta 'param'
        WEAK_BOOST,       // potencia los movimientos de potencia <= amount
        STAB_BOOST,       // bonificación por tipo propio (multiplier)
        NORMALIZE,        // sus movimientos pasan a ser de tipo normal
        RESIST_SUPER,     // recibe multiplier menos daño de lo supereficaz
        RESIST_FULL_HP,   // recibe multiplier menos daño con la vida completa
        SURVIVE,          // con la vida completa resiste un golpe mortal con 1 PS
        ABSORB_TYPE,      // inmune al tipo type_id y sube la estadística param
        CHIP,             // daño a quien lo golpea (% de sus PS máximos)
        PP_DRAIN,         // los movimientos rivales gastan 'amount' PP más
        CRIT_DAMAGE,      // multiplicador del daño crítico
        KLUTZ,            // su objeto no hace nada
        MOLD_BREAKER,     // ignora las habilidades del rival al atacar
        IGNORE_STAGES,    // ignora los niveles de estadística del rival
        WEATHER_NEGATE,   // anula el clima mientras esté en el campo
        FLINCH_IMMUNE,    // no se amedrenta
        PRIORITY_STATUS,  // los movimientos de estado ganan prioridad
        PROTEAN,          // cambia de tipo al del movimiento que usa
        REFLECT_STATUS,   // devuelve los movimientos de estado
        REFLECT_DROPS,    // devuelve las bajadas de estadística
        COPY_BOOSTS,      // copia las subidas de estadística del rival
        TRACE,            // copia la habilidad del rival
        TRANSFORM,        // se transforma en el rival
        COPY_ALLY,        // copia los niveles de estadística del aliado
        HEAL_ALLY,        // cura un % al aliado
        ILLUSION,         // el primer golpe que recibe lo esquiva y rompe la ilusión
        STANCE,           // postura: ataca fuerte al atacar y se protege con movimientos de estado
        HERO,             // tras salir del campo vuelve más fuerte
        SKILL_LINK        // sus movimientos encadenados golpean el máximo de veces
    };
    enum class Stat { ATTACK, DEFENSE, SP_ATTACK, SP_DEFENSE, SPEED, ACCURACY, EVASION, COUNT };
    enum class Status { NONE, BURN, SOAKED, ROOTED, PARALYSIS, FROZEN, CRUSHED, POISON, WEIGHED, DESTABILIZED, CONFUSED, INFESTED, CRACKED, POSSESSED, INDIGNANT, SWINDLED, RUSTED, BEWITCHED, STUNNED, COUNT };
    enum class Weather { NONE, RAIN, SUN, SAND, SNOW };
    enum class Cond { ANY, RAIN, SUN, SAND, SNOW, STATUS, FULL_HP };

    Kind kind = Kind::NONE;
    Stat stat = Stat::ATTACK;     // STAT_STAGE / STAT_MULT / ABSORB_TYPE: la estadística
    Status status = Status::NONE; // STATUS: el estado
    Weather weather = Weather::NONE; // WEATHER: el clima
    Cond cond = Cond::ANY;        // condición para que actúe
    bool self = false;
    float amount = 0.0f;          // niveles (STAT_STAGE, CRIT_BOOST), % (HEAL_SELF, RECOIL, DRAIN, CHIP, HEAL_ALLY), potencia (WEAK_BOOST), PP (PP_DRAIN)
    float chance = 100.0f;        // % de que ocurra (lo pone el movimiento o la habilidad)
    float multiplier = 1.0f;
    int typeId = -1;              // ABSORB_TYPE
    std::string param;            // etiqueta (TAG_BOOST)

    bool active() const { return kind != Kind::NONE; }

    static Kind parseKind(const std::string& t) {
        static const std::pair<const char*, Kind> N[] = {
            { "STAT_STAGE", Kind::STAT_STAGE }, { "STATUS", Kind::STATUS }, { "HEAL_SELF", Kind::HEAL_SELF }, { "RECOIL", Kind::RECOIL }, { "DRAIN", Kind::DRAIN }, { "PROTECT", Kind::PROTECT },
            { "CRIT_BOOST", Kind::CRIT_BOOST }, { "FLINCH", Kind::FLINCH }, { "ACCURACY_BOOST", Kind::ACCURACY_BOOST }, { "WEATHER", Kind::WEATHER }, { "STAT_MULT", Kind::STAT_MULT },
            { "EVASION_BOOST", Kind::EVASION_BOOST }, { "TAG_BOOST", Kind::TAG_BOOST }, { "WEAK_BOOST", Kind::WEAK_BOOST }, { "STAB_BOOST", Kind::STAB_BOOST }, { "NORMALIZE", Kind::NORMALIZE },
            { "RESIST_SUPER", Kind::RESIST_SUPER }, { "RESIST_FULL_HP", Kind::RESIST_FULL_HP }, { "SURVIVE", Kind::SURVIVE }, { "ABSORB_TYPE", Kind::ABSORB_TYPE }, { "CHIP", Kind::CHIP },
            { "PP_DRAIN", Kind::PP_DRAIN }, { "CRIT_DAMAGE", Kind::CRIT_DAMAGE }, { "KLUTZ", Kind::KLUTZ }, { "MOLD_BREAKER", Kind::MOLD_BREAKER }, { "IGNORE_STAGES", Kind::IGNORE_STAGES },
            { "WEATHER_NEGATE", Kind::WEATHER_NEGATE }, { "FLINCH_IMMUNE", Kind::FLINCH_IMMUNE }, { "PRIORITY_STATUS", Kind::PRIORITY_STATUS }, { "PROTEAN", Kind::PROTEAN },
            { "REFLECT_STATUS", Kind::REFLECT_STATUS }, { "REFLECT_DROPS", Kind::REFLECT_DROPS }, { "COPY_BOOSTS", Kind::COPY_BOOSTS }, { "TRACE", Kind::TRACE }, { "TRANSFORM", Kind::TRANSFORM },
            { "COPY_ALLY", Kind::COPY_ALLY }, { "HEAL_ALLY", Kind::HEAL_ALLY }, { "ILLUSION", Kind::ILLUSION }, { "STANCE", Kind::STANCE }, { "HERO", Kind::HERO }, { "SKILL_LINK", Kind::SKILL_LINK } };
        for (const auto& entry : N) if (t == entry.first) return entry.second;
        return Kind::NONE;
    }
    static Stat parseStat(const std::string& t) {
        return t == "DEFENSE" ? Stat::DEFENSE : t == "SP_ATTACK" ? Stat::SP_ATTACK : t == "SP_DEFENSE" ? Stat::SP_DEFENSE : t == "SPEED" ? Stat::SPEED : t == "ACCURACY" ? Stat::ACCURACY : t == "EVASION" ? Stat::EVASION : Stat::ATTACK;
    }
    // Código de cada estado (columna param de su efecto) y su nombre en pantalla; van en el mismo orden que el enum.
    static constexpr const char* STATUS_CODES[] = { "", "BURN", "SOAKED", "ROOTED", "PARALYSIS", "FROZEN", "CRUSHED", "POISON", "WEIGHED", "DESTABILIZED", "CONFUSED", "INFESTED", "CRACKED", "POSSESSED", "INDIGNANT", "SWINDLED", "RUSTED", "BEWITCHED", "STUNNED" };
    static constexpr const char* STATUS_NAMES[] = { "", "quemado", "empapado", "enraizado", "paralizado", "congelado", "molido", "envenenado", "lastrado", "desestabilizado", "confundido", "infestado", "agrietado", "poseído", "indignado", "estafado", "oxidado", "hechizado", "aturdido" };
    static Status parseStatus(const std::string& t) {
        for (int i = 1; i < static_cast<int>(Status::COUNT); ++i) if (t == STATUS_CODES[i]) return static_cast<Status>(i);
        return Status::NONE;
    }
    static Weather parseWeather(const std::string& t) { return t == "RAIN" ? Weather::RAIN : t == "SUN" ? Weather::SUN : t == "SAND" ? Weather::SAND : t == "SNOW" ? Weather::SNOW : Weather::NONE; }
    static Cond parseCond(const std::string& t) {
        return t == "RAIN" ? Cond::RAIN : t == "SUN" ? Cond::SUN : t == "SAND" ? Cond::SAND : t == "SNOW" ? Cond::SNOW : t == "STATUS" ? Cond::STATUS : t == "FULL_HP" ? Cond::FULL_HP : Cond::ANY;
    }
    static const char* statName(Stat s) {
        static constexpr const char* N[] = { "ataque", "defensa", "ataque especial", "defensa especial", "velocidad", "precisión", "evasión" };
        return N[static_cast<int>(s)];
    }
    static const char* statusName(Status s) { return STATUS_NAMES[static_cast<int>(s)]; }
    static const char* weatherName(Weather w) { return w == Weather::RAIN ? "lluvia" : w == Weather::SUN ? "sol" : w == Weather::SAND ? "tormenta de arena" : w == Weather::SNOW ? "nieve" : ""; }
};
