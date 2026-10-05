#pragma once
#include <string>
#include <vector>

// Una recompensa posible de un cofre: 'quantity' unidades del objeto 'itemId'.
struct ChestReward {
    int itemId = -1;
    int quantity = 1;
    float weight = 1.0f;
};

// Tipo de cofre: al abrirlo da una sola de sus recompensas, elegida al azar según su peso.
struct ChestType {
    int id = -1;
    std::string name;
    int rarity = 1; // 1 = común, 2 = raro, 3 = épico
    float spawnWeight = 1.0f;
    std::vector<ChestReward> rewards;
};

// Zona donde pueden aparecer cofres, con su límite diario.
struct ChestZone {
    int id = -1;
    float x = 0.0f;
    float z = 0.0f;
    int maxPerDay = 1;
};
