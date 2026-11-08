#pragma once
#include <string>
#include "inventory.hpp"
#include "playerProgress.hpp"
#include "pokemonStorage.hpp"
#include "../../../models/gameData.hpp"

// Uso de objetos sobre un pokémon (pociones, revivir, caramelo de nivel y piedras evolutivas). Es el único sitio donde se decide para qué
// sirve cada objeto y qué hace; lo usan por igual el menú de pokémon y la mochila.
namespace ItemUse {
    using Place = PokemonStorage::Place;
    constexpr int EVOLVE_REVIVE_PERCENT = 25; // PS con los que vuelve un debilitado al evolucionar por un objeto
    enum class Kind { NONE, HEAL, REVIVE, LEVEL, STONE };

    // Evolución que provoca esta piedra evolutiva en esta especie; nullptr si no hace nada.
    inline const Evolution* stoneEvolution(const GameData& data, const PokemonSpecies& species, const Item& item) {
        if (item.category != ItemCategory::STONE) return nullptr;
        for (const Evolution& evolution : species.evolutions) {
            if (evolution.method == Evolution::Method::STONE && evolution.itemId == item.id && data.speciesById(evolution.toId)) return &evolution;
        }
        return nullptr;
    }

    // ¿Es un objeto que se usa sobre un pokémon? Las piedras evolutivas tienen su propia categoría.
    inline Kind kind(const GameData& data, const Item& item) {
        if (item.category == ItemCategory::TRAINING) {
            const TrainingItem* type = data.training(item.refId);
            if (!type) return Kind::NONE;
            switch (type->effect) {
                case TrainingItem::Effect::HEAL:   return Kind::HEAL;
                case TrainingItem::Effect::REVIVE: return Kind::REVIVE;
                case TrainingItem::Effect::LEVEL:  return Kind::LEVEL;
                default:                           return Kind::NONE; // vitaminas: se gastan desde la ficha de estadísticas
            }
        }
        return item.category == ItemCategory::STONE ? Kind::STONE : Kind::NONE;
    }

    // ¿Tendría efecto sobre este pokémon?
    inline bool applies(const GameData& data, const PokemonStorage& storage, const PlayerProgress& progress, const Item& item, const OwnedPokemon& owned) {
        const PokemonSpecies* species = data.speciesById(owned.speciesId);
        switch (kind(data, item)) {
            case Kind::HEAL:   return !storage.fainted(owned) && storage.hpOf(owned) < storage.maxHp(owned);
            case Kind::REVIVE: return storage.fainted(owned);
            case Kind::LEVEL:  return owned.level < progress.levelCap();
            case Kind::STONE:  return species && stoneEvolution(data, *species, item);
            default:           return false;
        }
    }

    // Usa el objeto sobre el pokémon de 'place'. Si surte efecto gasta una unidad y devuelve el mensaje; si no, devuelve "" y no gasta nada.
    inline std::string apply(const GameData& data, PokemonStorage& storage, Inventory& inventory, const PlayerProgress& progress, const Item& item, const Place& place) {
        const std::vector<OwnedPokemon>& list = place.inTeam ? storage.team() : storage.pc();
        if (place.index < 0 || place.index >= static_cast<int>(list.size()) || inventory.count(item.id) <= 0) return {};
        const OwnedPokemon& owned = list[place.index];
        if (!applies(data, storage, progress, item, owned)) return {};
        const PokemonSpecies* before = data.speciesById(owned.speciesId);
        const std::string name = before ? before->name : "El pokémon";
        const bool wasFainted = storage.fainted(owned);
        const bool onCompanion = place.inTeam && place.index == storage.activeIndex() && !wasFainted; // regla única: sobre el acompañante no se cambia de pokémon
        const std::string itemName = data.itemTitle(item, 1);
        std::string message;
        switch (kind(data, item)) {
            case Kind::HEAL: {
                const TrainingItem* type = data.training(item.refId);
                storage.heal(place.inTeam, place.index, type->percent ? (std::max)(1, storage.maxHp(owned) * type->amount / 100) : type->amount);
                storage.noteMedicine(place.inTeam, place.index, type->id, false, onCompanion); // el mundo lo muestra sobre el acompañante
                message = itemName + " usado en " + name;
                break;
            }
            case Kind::REVIVE: {
                const TrainingItem* type = data.training(item.refId);
                storage.revive(place.inTeam, place.index, type->amount >= 100);
                storage.noteMedicine(place.inTeam, place.index, type->id, false, onCompanion);
                message = itemName + " usado en " + name;
                break;
            }
            case Kind::LEVEL: {
                const PokemonStorage::LevelResult result = storage.levelUp(place.inTeam, place.index, progress.levelCap());
                const PokemonSpecies* after = data.speciesById(list[place.index].speciesId);
                message = result == PokemonStorage::LevelResult::EVOLVED && after ? "¡" + name + " ha evolucionado a " + after->name + "!" : name + " sube de nivel";
                if (result == PokemonStorage::LevelResult::EVOLVED) {
                    if (wasFainted) storage.reviveEvolved(place.inTeam, place.index, EVOLVE_REVIVE_PERCENT);
                    storage.noteMedicine(place.inTeam, place.index, -1, true, onCompanion);
                }
                break;
            }
            case Kind::STONE: {
                const PokemonSpecies* after = storage.evolveWithStone(place.inTeam, place.index, item.id);
                if (!after) return {};
                if (wasFainted) storage.reviveEvolved(place.inTeam, place.index, EVOLVE_REVIVE_PERCENT);
                storage.noteMedicine(place.inTeam, place.index, -1, true, onCompanion);
                message = "¡" + name + " ha evolucionado a " + after->name + "!";
                break;
            }
            default: return {};
        }
        if (place.inTeam && !onCompanion) storage.setActive(place.index); // otro pokémon: el acompañante vuelve a su bola y sale el elegido
        inventory.add(item.id, -1);
        return message;
    }

    // Objetos equipables: se dan al pokémon de 'place' (el que llevaba otro lo devuelve a la mochila). No cambia al acompañante ni lleva animación.
    inline bool isHeld(const Item& item) { return item.category == ItemCategory::HELD; }

    inline std::string equip(const GameData& data, PokemonStorage& storage, Inventory& inventory, const Item& item, const Place& place) {
        const std::vector<OwnedPokemon>& list = place.inTeam ? storage.team() : storage.pc();
        if (!isHeld(item) || place.index < 0 || place.index >= static_cast<int>(list.size()) || inventory.count(item.id) <= 0 || list[place.index].heldItemId == item.id) return {};
        const PokemonSpecies* species = data.speciesById(list[place.index].speciesId);
        const int previous = storage.setHeld(place.inTeam, place.index, item.id);
        inventory.add(item.id, -1);
        if (previous >= 0) inventory.add(previous, 1);
        return data.itemTitle(item, 1) + " equipado a " + (species ? species->name : std::string("un pokémon"));
    }

    inline std::string unequip(const GameData& data, PokemonStorage& storage, Inventory& inventory, const Place& place) {
        const std::vector<OwnedPokemon>& list = place.inTeam ? storage.team() : storage.pc();
        if (place.index < 0 || place.index >= static_cast<int>(list.size()) || list[place.index].heldItemId < 0) return {};
        const int previous = storage.setHeld(place.inTeam, place.index, -1);
        inventory.add(previous, 1);
        const Item* item = data.item(previous);
        return (item ? data.itemTitle(*item, 1) : std::string("Objeto")) + " devuelto a la mochila";
    }
}
