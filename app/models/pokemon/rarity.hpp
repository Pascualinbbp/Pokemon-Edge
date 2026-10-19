#pragma once
#include <string>

// Rareza de una especie (tabla rarity). Cuanto mayor el id, más rara: aparece menos y da más experiencia al capturarla.
struct Rarity {
    int id = -1;
    std::string name;
    float spawnMultiplier = 1.0f;
    float xpMultiplier = 1.0f;
};
