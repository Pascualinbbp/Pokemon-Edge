#pragma once
#include <string>

// Categoría (tabla category): agrupa objetos, materiales y herramientas en la mochila.
struct ItemCategoryInfo {
    int id = -1;
    std::string name;
    std::string section = "Otros"; // apartado de la mochila que la agrupa (Pokémon, Recursos, Herramientas...)
    int iconItemId = -1; // objeto que da icono a su pestaña (el primero de la categoría)
};
