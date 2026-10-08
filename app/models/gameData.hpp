#pragma once
#include <string>
#include <vector>
#include "pokemon/ability.hpp"
#include "world/chestType.hpp"
#include "world/groundItemType.hpp"
#include "world/habitat.hpp"
#include "world/weather.hpp"
#include "items/item.hpp"
#include "items/itemCategoryInfo.hpp"
#include "items/material.hpp"
#include "items/pokeballType.hpp"
#include "pokemon/pokemonType.hpp"
#include "pokemon/pokemonSpecies.hpp"
#include "world/resourceNodeType.hpp"
#include "pokemon/skill.hpp"
#include "items/tool.hpp"
#include "items/trainingItem.hpp"

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
    std::vector<PokemonType> types;
    std::vector<GroundItemType> groundItems;
    std::vector<TrainingItem> trainingItems;
    std::vector<ChestType> chests;
    std::vector<ResourceNodeType> nodes;
    std::vector<Habitat> habitats;
    std::vector<Weather> weathers;
    std::vector<WeatherFusion> fusions;

    static const GameData& empty() {
        static const GameData data;
        return data;
    }

    const PokeballType* ball(int id) const { return find(balls, id); }
    const Material* material(int id) const { return find(materials, id); }
    const Tool* tool(int id) const { return find(tools, id); }
    const Skill* skill(int id) const { return find(skills, id); }
    const Item* item(int id) const { return find(items, id); }
    const TrainingItem* training(int id) const { return find(trainingItems, id); }
    const Ability* ability(int id) const { return find(abilities, id); }
    const PokemonType* type(int id) const { return find(types, id); }
    const PokemonSpecies* speciesById(int id) const { return find(species, id); }
    const Habitat* habitat(int id) const { return find(habitats, id); }
    const Weather* weather(int id) const { return find(weathers, id); }

    int habitatIndex(int id) const {
        for (size_t i = 0; i < habitats.size(); ++i) if (habitats[i].id == id) return static_cast<int>(i);
        return -1;
    }

    // Clima que resulta de juntar dos (-1 si no se fusionan); el orden no importa.
    int fusionOf(int a, int b) const {
        for (const WeatherFusion& fusion : fusions) if ((fusion.a == a && fusion.b == b) || (fusion.a == b && fusion.b == a)) return fusion.result;
        return -1;
    }

    // Pokéball exclusiva del pokémon inicial (la que no existe como objeto). nullptr si no hay.
    const PokeballType* starterBall() const {
        for (const PokeballType& entry : balls) if (!entry.obtainable) return &entry;
        return nullptr;
    }

    const PokeballType* ballByName(const std::string& name) const {
        for (const PokeballType& entry : balls) if (entry.name == name) return &entry;
        return nullptr;
    }

    // Última forma de la línea evolutiva de una especie (ella misma si no evoluciona).
    const PokemonSpecies* finalForm(const PokemonSpecies& from) const {
        const PokemonSpecies* current = &from;
        for (int guard = 0; guard < 8 && current->evolvesToId > 0; ++guard) {
            const PokemonSpecies* next = speciesById(current->evolvesToId);
            if (!next) break;
            current = next;
        }
        return current;
    }

    // El objeto de entrenamiento con ese efecto (nullptr si no existe) y su objeto de inventario.
    const TrainingItem* trainingWith(TrainingItem::Effect effect) const {
        for (const TrainingItem& entry : trainingItems) if (entry.effect == effect) return &entry;
        return nullptr;
    }

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

    // ¿Alguna de sus habilidades lo hace levitar?
    bool levitates(const PokemonSpecies& species) const {
        const Ability* active = ability(species.abilityId);
        const Ability* passive = ability(species.passiveId);
        return (active && active->floats) || (passive && passive->floats);
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
            case ItemCategory::TRAINING: if (const TrainingItem* type = training(item.refId)) return type->name; break;
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
            case ItemCategory::TRAINING: if (const TrainingItem* type = training(item.refId)) return type->description; break;
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
