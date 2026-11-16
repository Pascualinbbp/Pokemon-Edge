#pragma once
#include <string>
#include "effectSpec.hpp"

// Habilidad de combate (tabla ability). Efecto: NONE · FLOAT (el pokémon levita) · WATER_BREATHING (respira bajo el agua y da una burbuja de aire al jugador, con el pokémon en el equipo) · BIOLUMINESCENCE (el jugador brilla en la oscuridad, con el pokémon en el equipo; en combate potencia en 'multiplier' los movimientos de luz) · TYPE_BOOST (potencia los ataques del tipo 'typeId'
// por 'multiplier' mientras su vida está por debajo de la fracción 'hpBelow').
struct Ability {
    enum class Effect { NONE, FLOAT, WATER_BREATHING, BIOLUMINESCENCE, TYPE_BOOST };

    int id = -1;
    std::string name;
    std::string description;
    Effect effect = Effect::NONE;
    int typeId = -1;
    float multiplier = 1.0f;
    float hpBelow = 1.0f;
    enum class Trigger { PASSIVE, ON_ENTER, ON_CONTACT, END_TURN, ON_HIT, ON_KO, ON_STAT_DROP, ON_CRIT_TAKEN, ON_FLINCH }; // cuándo actúa en combate (ON_HIT = recibe un golpe, ON_KO = deja fuera de combate, ON_STAT_DROP = le bajan una estadística, ON_CRIT_TAKEN = recibe un crítico, ON_FLINCH = se amedrenta)
    Trigger trigger = Trigger::PASSIVE;
    int priority = 0;  // orden entre habilidades que se activan a la vez (mayor, antes)
    EffectSpec spec;   // efecto de combate

    // Las habilidades pasivas son solo de diseño (levitar, respirar bajo el agua): un plus que una especie puede tener o no y que nunca actúa como activa.
    // Todo lo demás son activas, que tiene todo pokémon.
    bool passiveKind() const { return effect == Effect::FLOAT || effect == Effect::WATER_BREATHING || effect == Effect::BIOLUMINESCENCE; }
    bool glows() const { return effect == Effect::BIOLUMINESCENCE; }
    bool floats() const { return effect == Effect::FLOAT; }
    bool breathesWater() const { return effect == Effect::WATER_BREATHING; }

    static Trigger parseTrigger(const std::string& t) {
        return t == "ON_ENTER" ? Trigger::ON_ENTER : t == "ON_CONTACT" ? Trigger::ON_CONTACT : t == "END_TURN" ? Trigger::END_TURN : t == "ON_HIT" ? Trigger::ON_HIT : t == "ON_KO" ? Trigger::ON_KO :
               t == "ON_STAT_DROP" ? Trigger::ON_STAT_DROP : t == "ON_CRIT_TAKEN" ? Trigger::ON_CRIT_TAKEN : t == "ON_FLINCH" ? Trigger::ON_FLINCH : Trigger::PASSIVE;
    }
    static Effect parse(const std::string& text) { return text == "FLOAT" ? Effect::FLOAT : text == "WATER_BREATHING" ? Effect::WATER_BREATHING : text == "BIOLUMINESCENCE" ? Effect::BIOLUMINESCENCE : text == "TYPE_BOOST" ? Effect::TYPE_BOOST : Effect::NONE; }
};
