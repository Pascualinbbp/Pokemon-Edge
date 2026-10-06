#pragma once
#include <string>

// Herramienta (tabla tool): su posesión da una habilidad de trabajo al jugador.
struct Tool {
    int id = -1;
    std::string name;
    std::string description;
    int skillId = -1;
};
