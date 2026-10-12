#pragma once
#include <string>

// Habilidad de trabajo (tabla skill): talar, picar...
struct Skill {
    int id = -1;
    std::string name;
    std::string description;
    std::string animation; // PlayerAction que hace el jugador al usarla (CHOP, MINE, WATER, FISH...)
};
