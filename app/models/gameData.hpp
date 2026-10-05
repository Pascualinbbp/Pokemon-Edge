#pragma once
#include <string>
#include <vector>
#include "chestType.hpp"
#include "item.hpp"
#include "material.hpp"
#include "pokeballType.hpp"
#include "resourceNodeType.hpp"

// Todos los datos de juego que viven en la base de datos, cargados una sola vez al arrancar.
struct GameData {
    std::vector<Item> items;
    std::vector<PokeballType> balls;
    std::vector<Material> materials;
    std::vector<ChestType> chests;
    std::vector<ResourceNodeType> nodes;

    static const GameData& empty() {
        static const GameData data;
        return data;
    }

    const PokeballType* ball(int id) const { return find(balls, id); }
    const Material* material(int id) const { return find(materials, id); }
    const Item* item(int id) const { return find(items, id); }

    // El objeto que representa un material (para dárselo al inventario). nullptr si no está en la tabla item.
    const Item* materialItem(int materialId) const {
        for (const Item& entry : items) if (entry.category == ItemCategory::MATERIAL && entry.refId == materialId) return &entry;
        return nullptr;
    }

    // Nombre del objeto, que vive en la tabla de su categoría.
    const std::string& itemName(const Item& item) const {
        static const std::string unknown = "?";
        switch (item.category) {
            case ItemCategory::POKEBALL: if (const PokeballType* type = ball(item.refId)) return type->name; break;
            case ItemCategory::MATERIAL: if (const Material* type = material(item.refId)) return type->name; break;
            default: break;
        }
        return unknown;
    }

    private:
    template <typename T>
    static const T* find(const std::vector<T>& list, int id) {
        for (const T& entry : list) if (entry.id == id) return &entry;
        return nullptr;
    }
};
