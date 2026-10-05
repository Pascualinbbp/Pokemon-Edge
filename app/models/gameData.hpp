#pragma once
#include <vector>
#include "chestType.hpp"
#include "item.hpp"
#include "pokeballType.hpp"

// Todos los datos de juego que viven en la base de datos, cargados una sola vez al arrancar.
struct GameData {
    std::vector<Item> items;
    std::vector<PokeballType> balls;
    std::vector<ChestType> chests;
    std::vector<ChestZone> zones;

    static const GameData& empty() {
        static const GameData data;
        return data;
    }

    const PokeballType* ball(int id) const {
        for (const PokeballType& type : balls) if (type.id == id) return &type;
        return nullptr;
    }

    const Item* item(int id) const {
        for (const Item& entry : items) if (entry.id == id) return &entry;
        return nullptr;
    }
};
