#pragma once
#include <string>

// Categorías de objeto (tabla item_category). Cada una tiene su propia tabla de datos; item.ref_id apunta a ella.
enum class ItemCategory { UNKNOWN, POKEBALL };

namespace ItemCategoryText {
    inline ItemCategory parse(const std::string& text) {
        if (text == "POKEBALL") return ItemCategory::POKEBALL;
        return ItemCategory::UNKNOWN;
    }
}

// Objeto del juego (tabla item): su nombre y demás datos están en la tabla de su categoría (ver GameData).
struct Item {
    int id = -1;
    ItemCategory category = ItemCategory::UNKNOWN;
    int refId = -1; // id en la tabla de su categoría
};
