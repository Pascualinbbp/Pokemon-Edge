#pragma once
#include <string>

// Categorías de objeto (tabla item_category). Cada una tiene su propia tabla de datos; item.ref_id apunta a ella.
enum class ItemCategory { UNKNOWN, POKEBALL, MATERIAL, TOOL };

namespace ItemCategoryText {
    inline ItemCategory parse(const std::string& text) {
        if (text == "POKEBALL") return ItemCategory::POKEBALL;
        if (text == "MATERIAL") return ItemCategory::MATERIAL;
        if (text == "TOOL") return ItemCategory::TOOL;
        return ItemCategory::UNKNOWN;
    }
}

// Objeto del juego (tabla item): su nombre y demás datos están en la tabla de su categoría (ver GameData).
struct Item {
    int id = -1;
    ItemCategory category = ItemCategory::UNKNOWN;
    int categoryId = -1; // id en item_category (para agrupar en el inventario)
    int refId = -1; // id en la tabla de su categoría
};
