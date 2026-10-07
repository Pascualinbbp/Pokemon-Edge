#pragma once
#include <string>

// Tipo de Pokéball tal como está en la base de datos.
struct PokeballType {
    int id = -1;
    std::string name;
    std::string description;
    float captureMultiplier = 1.0f; // multiplica el porcentaje de captura
    bool obtainable = true;         // false = bola exclusiva (la del pokémon inicial): no es un objeto
};
