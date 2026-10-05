#pragma once
#include <string>

// Categorías de objeto. Cada una tiene su propia tabla en la base de datos; item.ref_id apunta a ella.
enum class ItemCategory { UNKNOWN, POKEBALL };

namespace ItemCategoryText {
    inline ItemCategory parse(const std::string& text) {
        if (text == "POKEBALL") return ItemCategory::POKEBALL;
        return ItemCategory::UNKNOWN;
    }
}

// Objeto del juego (vista item_info): lo que tienen en común todas las categorías.
struct Item {
    int id = -1;
    ItemCategory category = ItemCategory::UNKNOWN;
    int refId = -1; // id en la tabla de su categoría
    std::string name;
    std::string description;
};
