#pragma once
#include <string>

// Habilidad de combate (tabla ability). Efecto: NONE · FLOAT (el pokémon levita) · TYPE_BOOST (potencia los ataques del tipo 'typeId'
// por 'multiplier' mientras su vida está por debajo de la fracción 'hpBelow').
struct Ability {
    enum class Effect { NONE, FLOAT, TYPE_BOOST };

    int id = -1;
    std::string name;
    std::string description;
    Effect effect = Effect::NONE;
    int typeId = -1;
    float multiplier = 1.0f;
    float hpBelow = 1.0f;

    bool floats() const { return effect == Effect::FLOAT; }

    static Effect parse(const std::string& text) { return text == "FLOAT" ? Effect::FLOAT : text == "TYPE_BOOST" ? Effect::TYPE_BOOST : Effect::NONE; }
};
