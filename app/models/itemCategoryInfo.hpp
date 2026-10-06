#pragma once
#include <string>

// Categoría de objeto (tabla item_category): 'name' es la clave que reconoce el código y 'label' lo que ve el jugador.
struct ItemCategoryInfo {
    int id = -1;
    std::string name;
    std::string label;
};
