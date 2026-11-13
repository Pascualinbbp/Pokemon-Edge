#pragma once
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include "../utils/core/randomUtil.hpp"
#include "pokemon/ability.hpp"
#include "pokemon/move.hpp"
#include "pokemon/dexReward.hpp"
#include "pokemon/generation.hpp"
#include "pokemon/nature.hpp"
#include "pokemon/rarity.hpp"
#include "pokemon/typeChart.hpp"
#include "world/chestType.hpp"
#include "world/reward.hpp"
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
#include "items/heldItem.hpp"

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
    std::vector<Move> moves;
    std::vector<MoveTag> moveTags;
    std::vector<PokemonType> types;
    TypeChart typeChart;
    std::vector<Rarity> rarities;
    std::vector<Generation> generations;
    std::vector<Nature> natures;
    std::vector<RewardEntry> rewards; // ordenadas por grupo
    std::vector<TrainingItem> trainingItems;
    std::vector<HeldItem> heldItems;
    std::vector<ChestType> chests;
    std::vector<ResourceNodeType> nodes;
    std::vector<DexReward> dexRewards;
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
    const HeldItem* held(int id) const { return id < 0 ? nullptr : find(heldItems, id); }
    const Ability* ability(int id) const { return find(abilities, id); }
    const Move* move(int id) const { return id < 0 ? nullptr : find(moves, id); }
    const Move* moveByName(const std::string& name) const { for (const Move& m : moves) if (m.name == name) return &m; return nullptr; }
    const PokemonType* type(int id) const { return find(types, id); }
    const Rarity* rarity(int id) const { return find(rarities, id); }
    const Generation* generation(int id) const { return find(generations, id); }
    const Nature* nature(int id) const { return id <= 0 ? nullptr : find(natures, id); } // 0 = sin naturaleza (neutra)
    // Deja las habilidades de cada especie como deben ser: la activa es obligatoria (y nunca una pasiva), la pasiva es opcional (y solo de las pasivas).
    // Una activa con efecto de pasiva pasa al hueco de pasiva; una pasiva con efecto de activa, al de activa; la lista de activas posibles solo lleva activas.
    void normalizeAbilities() {
        for (PokemonSpecies& s : species) {
            const Ability* active = ability(s.abilityId);
            const Ability* passive = ability(s.passiveId);
            if (active && active->passiveKind()) { if (!passive) s.passiveId = active->id; s.abilityId = -1; }
            if (passive && !passive->passiveKind()) { if (s.abilityId < 0 && !s.abilityPool.empty()) s.abilityId = passive->id; s.passiveId = -1; }
            std::vector<int> pool;
            for (const int id : s.abilityPool) { const Ability* a = ability(id); if (a && !a->passiveKind() && std::find(pool.begin(), pool.end(), id) == pool.end()) pool.push_back(id); }
            if (pool.empty() && s.abilityId >= 0) pool.push_back(s.abilityId);
            s.abilityPool = pool;
            if (!pool.empty() && (s.abilityId < 0 || std::find(pool.begin(), pool.end(), s.abilityId) == pool.end())) s.abilityId = pool.front();
        }
    }
    // Habilidad activa de un ejemplar: la que tiene si es de las de su especie; si no, una al azar de las posibles.
    int abilityFor(const PokemonSpecies& s, int chosen) const {
        if (chosen >= 0 && std::find(s.abilityPool.begin(), s.abilityPool.end(), chosen) != s.abilityPool.end()) return chosen;
        if (s.abilityPool.empty()) return s.abilityId;
        return s.abilityPool[static_cast<size_t>(RandomUtil::integer(0, static_cast<int>(s.abilityPool.size()) - 1))];
    }

    // Id de una naturaleza al azar (0 si no hay ninguna cargada).
    int randomNatureId() const { return natures.empty() ? 0 : natures[static_cast<size_t>(RandomUtil::integer(0, static_cast<int>(natures.size()) - 1))].id; }
    const Rarity* rarityOf(const PokemonSpecies& species) const { return rarity(species.rarityId); }
    float spawnWeight(const ChestType& chest) const { const Rarity* r = rarity(chest.rarity); return r ? r->spawnMultiplier : 1.0f; }
    float spawnMultiplier(const PokemonSpecies& species) const { const Rarity* r = rarityOf(species); return r ? r->spawnMultiplier : 1.0f; }
    float xpMultiplier(const PokemonSpecies& species) const { const Rarity* r = rarityOf(species); return r ? r->xpMultiplier : 1.0f; }
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
        for (int guard = 0; guard < 8 && !current->evolutions.empty(); ++guard) {
            const PokemonSpecies* next = speciesById(current->evolutions.front().toId);
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

    // El objeto del inventario de un material, de una herramienta o de un objeto con efecto (nullptr si no existe).
    const Item* materialItem(int materialId) const { return item(MATERIAL_ITEM_BASE + materialId); }
    const Item* toolItem(int toolId) const { return item(TOOL_ITEM_BASE + toolId); }

    // Entradas de un grupo de recompensas (tabla reward).
    std::pair<const RewardEntry*, const RewardEntry*> rewardGroup(int groupId) const {
        const RewardEntry* begin = rewards.data();
        const RewardEntry* end = begin + rewards.size();
        const RewardEntry* first = std::find_if(begin, end, [&](const RewardEntry& e) { return e.groupId == groupId; });
        const RewardEntry* last = first;
        while (last != end && last->groupId == groupId) ++last;
        return { first, last };
    }

    // El objeto de inventario que da una entrada de recompensa (nullptr si es experiencia o no existe).
    const Item* rewardItem(const RewardEntry& entry) const {
        return entry.kind == RewardEntry::Kind::ITEM ? item(entry.targetId) : entry.kind == RewardEntry::Kind::MATERIAL ? materialItem(entry.targetId) : nullptr;
    }

    // ¿Alguna de sus habilidades lo hace levitar?
    bool levitates(const PokemonSpecies& species) const {
        const Ability* active = ability(species.abilityId);
        const Ability* passive = ability(species.passiveId);
        return (active && active->floats()) || (passive && passive->floats());
    }

    // ¿Alguna de sus habilidades le permite respirar bajo el agua (y dar aire al jugador)?
    bool breathesUnderwater(const PokemonSpecies& species) const {
        const Ability* active = ability(species.abilityId);
        const Ability* passive = ability(species.passiveId);
        return (active && active->breathesWater()) || (passive && passive->breathesWater());
    }

    // La herramienta que representa un objeto (nullptr si no es una herramienta).
    const Tool* toolOf(const Item& item) const { return item.category == ItemCategory::TOOL ? tool(item.refId) : nullptr; }

    const std::string& itemName(const Item& item) const { return item.name; }

    // Nombre que ve el jugador: el de la herramienta cambia con su nivel ('count' = unidades o nivel); el resto, su nombre.
    std::string itemTitle(const Item& item, int count) const {
        if (const Tool* found = toolOf(item)) return found->title(count);
        return item.name;
    }

    std::string itemText(const Item& item, int count) const {
        if (const Tool* found = toolOf(item)) return found->effects(count);
        return item.description;
    }

    private:
    template <typename T>
    static const T* find(const std::vector<T>& list, int id) {
        for (const T& entry : list) if (entry.id == id) return &entry;
        return nullptr;
    }
};
