#pragma once
#include <string>

// Material de recolección (tabla material).
struct Material {
    int id = -1;
    std::string name;
    std::string description;
    int rgb = -1; // color del bloque (0xRRGGBB, columna color de la tabla material); -1 si falta
};
