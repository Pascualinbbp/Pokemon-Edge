#pragma once
#include <string>

// Habilidad de combate (tabla ability). 'floats': el pokémon que la tiene levita.
struct Ability {
    int id = -1;
    std::string name;
    std::string description;
    bool floats = false;
};
