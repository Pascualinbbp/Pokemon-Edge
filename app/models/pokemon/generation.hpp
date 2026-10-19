#pragma once
#include <string>

// Generación de pokémon (tabla generation) y la región a la que pertenece.
struct Generation {
    int id = -1;
    std::string name;
    std::string region;
};
