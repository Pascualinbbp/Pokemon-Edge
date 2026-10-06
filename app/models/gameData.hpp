#pragma once
#include <string>
#include <vector>
#include "chestType.hpp"
#include "item.hpp"
#include "itemCategoryInfo.hpp"
#include "material.hpp"
#include "pokeballType.hpp"
#include "pokemonSpecies.hpp"
#include "resourceNodeType.hpp"
#include "skill.hpp"
#include "tool.hpp"

// Todos los datos de juego que viven en la base de datos, cargados una sola vez al arrancar.
struct GameData {
    std::vector<Item> items;
    std::vector<ItemCategoryInfo> categories;
    std::vector<PokeballType> balls;
    std::vector<Material> materials;
    std::vector<Tool> tools;
    std::vector<Skill> skills;
    std::vector<PokemonSpecies> species;
    std::vector<ChestType> chests;
    std::vector<ResourceNodeType> nodes;

    static const GameData& empty() {
        static const GameData data;
        return data;
    }

    const PokeballType* ball(int id) const { return find(balls, id); }
    const Material* material(int id) const { return find(materials, id); }
    const Tool* tool(int id) const { return find(tools, id); }
    const Skill* skill(int id) const { return find(skills, id); }
    const Item* item(int id) const { return find(items, id); }
    const PokemonSpecies* speciesById(int id) const { return find(species, id); }

    int speciesIndex(int id) const {
        for (size_t i = 0; i < species.size(); ++i) if (species[i].id == id) return static_cast<int>(i);
        return -1;
    }

    int speciesIndexByName(const std::string& name) const {
        for (size_t i = 0; i < species.size(); ++i) if (species[i].name == name) return static_cast<int>(i);
        return -1;
    }

    // El objeto que representa un material (para dárselo al inventario). nullptr si no está en la tabla item.
    const Item* materialItem(int materialId) const {
        for (const Item& entry : items) if (entry.category == ItemCategory::MATERIAL && entry.refId == materialId) return &entry;
        return nullptr;
    }

    // Nombre y descripción del objeto: viven en la tabla de su categoría.
    const std::string& itemName(const Item& item) const {
        static const std::string unknown = "?";
        switch (item.category) {
            case ItemCategory::POKEBALL: if (const PokeballType* type = ball(item.refId)) return type->name; break;
            case ItemCategory::MATERIAL: if (const Material* type = material(item.refId)) return type->name; break;
            case ItemCategory::TOOL:     if (const Tool* type = tool(item.refId)) return type->name; break;
            default: break;
        }
        return unknown;
    }

    const std::string& itemDescription(const Item& item) const {
        static const std::string none;
        switch (item.category) {
            case ItemCategory::POKEBALL: if (const PokeballType* type = ball(item.refId)) return type->description; break;
            case ItemCategory::MATERIAL: if (const Material* type = material(item.refId)) return type->description; break;
            case ItemCategory::TOOL:     if (const Tool* type = tool(item.refId)) return type->description; break;
            default: break;
        }
        return none;
    }

    private:
    template <typename T>
    static const T* find(const std::vector<T>& list, int id) {
        for (const T& entry : list) if (entry.id == id) return &entry;
        return nullptr;
    }
};
