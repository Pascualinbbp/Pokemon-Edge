#pragma once
#include <string>

// Material de recolección: recurso que se extrae del mundo (tabla material).
struct Material {
    int id = -1;
    std::string name;
    std::string description;
    int categoryId = -1;
    int buyPrice = 0;
    int sellPrice = 0;
    int rgb = -1; // color del bloque (0xRRGGBB, columna color de la tabla material); -1 si falta
};
