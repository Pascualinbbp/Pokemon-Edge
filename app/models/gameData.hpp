#pragma once
#include <string>
#include <vector>
#include "ability.hpp"
#include "chestType.hpp"
#include "element.hpp"
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
    std::vector<Ability> abilities;
    std::vector<Element> elements;
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
    const Ability* ability(int id) const { return find(abilities, id); }
    const Element* element(int id) const { return find(elements, id); }
    const PokemonSpecies* speciesById(int id) const { return find(species, id); }

    int speciesIndex(int id) const {
        for (size_t i = 0; i < species.size(); ++i) if (species[i].id == id) return static_cast<int>(i);
        return -1;
    }

    int speciesIndexByName(const std::string& name) const {
        for (size_t i = 0; i < species.size(); ++i) if (species[i].name == name) return static_cast<int>(i);
        return -1;
    }

    // El objeto que representa una fila de la tabla de su categoría. nullptr si no está en la tabla item.
    const Item* itemOf(ItemCategory category, int refId) const {
        for (const Item& entry : items) if (entry.category == category && entry.refId == refId) return &entry;
        return nullptr;
    }

    const Item* materialItem(int materialId) const { return itemOf(ItemCategory::MATERIAL, materialId); }
    const Item* toolItem(int toolId) const { return itemOf(ItemCategory::TOOL, toolId); }

    // ¿Alguna de sus habilidades pasivas lo hace levitar?
    bool levitates(const PokemonSpecies& species) const {
        for (const SpeciesAbility& entry : species.abilities) {
            if (!entry.passive) continue;
            if (const Ability* found = ability(entry.abilityId); found && found->floats) return true;
        }
        return false;
    }

    // La herramienta que representa un objeto (nullptr si no es una herramienta).
    const Tool* toolOf(const Item& item) const { return item.category == ItemCategory::TOOL ? tool(item.refId) : nullptr; }

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

    // Nombre que ve el jugador: el de la herramienta cambia con su nivel ('count' = unidades o nivel); el resto, su nombre.
    std::string itemTitle(const Item& item, int count) const {
        if (const Tool* found = toolOf(item)) if (const ToolTier* tier = found->tier(count)) return tier->name;
        return itemName(item);
    }

    std::string itemText(const Item& item, int count) const {
        if (const Tool* found = toolOf(item)) if (const ToolTier* tier = found->tier(count)) return tier->description;
        return itemDescription(item);
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
