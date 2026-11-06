#pragma once
#include <string>

// Categoría (tabla category): agrupa objetos, materiales y herramientas en la mochila.
struct ItemCategoryInfo {
    int id = -1;
    std::string name;
    int iconItemId = -1; // objeto que da icono a su pestaña (el primero de la categoría)
};
