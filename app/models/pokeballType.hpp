#pragma once
#include <string>

// Tipo de Pokéball tal como está en la base de datos.
struct PokeballType {
    int id = -1;
    std::string name;
    float captureMultiplier = 1.0f; // multiplica el porcentaje de captura
    float r = 1.0f, g = 1.0f, b = 1.0f; // color de la bola (0..1)
    int startingStock = 0;          // unidades con las que empieza una partida nueva
};
