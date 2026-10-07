#pragma once
#include <string>
#include <vector>

// Una recompensa posible de un cofre: entre 'minQuantity' y 'maxQuantity' unidades del objeto 'itemId' (al azar),
// con su probabilidad (%) dentro del cofre.
struct ChestReward {
    int itemId = -1;
    int minQuantity = 1;
    int maxQuantity = 1;
    float probability = 1.0f;
};

// Tipo de cofre: al abrirlo da una sola de sus recompensas, elegida al azar según su probabilidad.
struct ChestType {
    int id = -1;
    std::string name;
    int rarity = 1; // id de chest_rarity: 1 = común, 2 = raro, 3 = épico
    float spawnWeight = 1.0f;
    std::vector<ChestReward> rewards;
};
