#pragma once
#include <string>
#include "effectSpec.hpp"

// Habilidad de combate (tabla ability). Efecto: NONE · FLOAT (el pokémon levita) · WATER_BREATHING (respira bajo el agua y da una burbuja de aire al jugador) · TYPE_BOOST (potencia los ataques del tipo 'typeId'
// por 'multiplier' mientras su vida está por debajo de la fracción 'hpBelow').
struct Ability {
    enum class Effect { NONE, FLOAT, WATER_BREATHING, TYPE_BOOST };

    int id = -1;
    std::string name;
    std::string description;
    Effect effect = Effect::NONE;
    int typeId = -1;
    float multiplier = 1.0f;
    float hpBelow = 1.0f;
    enum class Trigger { PASSIVE, ON_ENTER, ON_CONTACT, END_TURN }; // cuándo actúa en combate
    Trigger trigger = Trigger::PASSIVE;
    int priority = 0;  // orden entre habilidades que se activan a la vez (mayor, antes)
    EffectSpec spec;   // efecto de combate

    bool floats() const { return effect == Effect::FLOAT; }
    bool breathesWater() const { return effect == Effect::WATER_BREATHING; }

    static Trigger parseTrigger(const std::string& t) { return t == "ON_ENTER" ? Trigger::ON_ENTER : t == "ON_CONTACT" ? Trigger::ON_CONTACT : t == "END_TURN" ? Trigger::END_TURN : Trigger::PASSIVE; }
    static Effect parse(const std::string& text) { return text == "FLOAT" ? Effect::FLOAT : text == "WATER_BREATHING" ? Effect::WATER_BREATHING : text == "TYPE_BOOST" ? Effect::TYPE_BOOST : Effect::NONE; }
};
