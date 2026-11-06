#pragma once
#include <string>

// Comportamiento de un objeto en el código; sale del efecto de su fila en item (o de ser material / herramienta).
enum class ItemCategory { UNKNOWN, POKEBALL, MATERIAL, STONE, TOOL, TRAINING };

// ¿Su ref_id apunta a la tabla material? (solo los materiales)
inline bool usesMaterialTable(ItemCategory category) { return category == ItemCategory::MATERIAL; }

// Ids de los objetos que representan materiales y herramientas en el inventario (la tabla item solo tiene objetos con efecto).
inline constexpr int MATERIAL_ITEM_BASE = 1000;
inline constexpr int TOOL_ITEM_BASE = 2000;

// Objeto del inventario: una fila de item, o un material / herramienta (que tienen su propia tabla).
struct Item {
    int id = -1;
    ItemCategory category = ItemCategory::UNKNOWN;
    int categoryId = -1;   // id en la tabla category (para agrupar en la mochila)
    int refId = -1;        // POKEBALL: id de la bola · TRAINING / STONE: id del objeto · MATERIAL / TOOL: id en su tabla
    int typeId = -1;       // piedras evolutivas: tipo de la piedra
    std::string name;
    std::string description;
    std::string asset;     // "items/<categoría>/<id>": modelo assets/models/<asset>.glb y sprite assets/sprites/<asset>.png
    int buyPrice = 0;      // coste de compra en pokémonedas (0 = no se compra)
    int sellPrice = 0;     // coste de venta (0 = no se vende)
    int giftEvery = 0;     // la tienda regala cada tantas unidades compradas...
    int giftRewardId = -1; // ...la recompensa de este grupo (tabla reward)
};
