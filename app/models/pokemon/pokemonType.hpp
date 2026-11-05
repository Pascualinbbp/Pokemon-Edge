#pragma once
#include <string>

struct PokemonType {
    int id = -1;
    std::string name;
    int rgb = -1; // color de la etiqueta (0xRRGGBB, columna color de la tabla type); -1 si falta
};
