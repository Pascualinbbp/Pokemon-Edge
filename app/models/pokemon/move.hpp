#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "effectSpec.hpp"

// Clase de movimiento (tabla move_class): físico, especial o de estado. Una clase tiene muchos movimientos (1:n).
struct MoveClass {
    enum class Kind { PHYSICAL, SPECIAL, STATUS };
    int id = -1;
    std::string name;
    Kind kind = Kind::PHYSICAL;
    static Kind parse(const std::string& name) { return name == "Especial" ? Kind::SPECIAL : name == "Estado" ? Kind::STATUS : Kind::PHYSICAL; }
};

// Etiqueta de movimiento (tabla move_tag): contacto, puño, suelo, prioridad... Un movimiento tiene varias y una etiqueta está en varios (n:m).
struct MoveTag {
    int id = -1;
    std::string name;
    int priority = 0; // las etiquetas de prioridad suman aquí su nivel
};

// Movimiento (tabla move).
struct Move {
    int id = -1;
    std::string name;
    std::string description;
    int typeId = -1;
    int classId = -1;
    MoveClass::Kind kind = MoveClass::Kind::PHYSICAL;
    int pp = 10;
    int power = 0;     // 0 = no hace daño
    int accuracy = 0;  // 0 = no falla nunca
    EffectSpec effect;
    float learnWeight = 10.0f;     // probabilidad relativa de que un pokémon lo intente aprender
    std::vector<int> tags;         // id de move_tag
    std::vector<std::string> tagNames;
    int priority = 0;              // suma de las etiquetas de prioridad
    // Alcance (etiquetas del movimiento): NORMAL = un rival (o él mismo si es propio), ALL_OTHERS = todos los demás del campo ("Todos los demás"), FOES = los dos rivales ("Ambos rivales"), ANY = el que se elija, aliado o él mismo incluidos ("Cualquier objetivo").
    enum class Scope { NORMAL, ALL_OTHERS, FOES, ANY };
    Scope scope = Scope::NORMAL;
    bool contact = false;          // etiqueta "Contacto"
    std::vector<int> learnTags;    // si no está vacía, solo lo aprenden los pokémon con alguna de estas etiquetas (sea cual sea su tipo)

    bool damaging() const { return kind != MoveClass::Kind::STATUS && power > 0; }
    bool hasLearnTag(int id) const { return std::find(learnTags.begin(), learnTags.end(), id) != learnTags.end(); }
};
