#pragma once
#include <string>

// Tipo de cofre (tabla chest): al abrirlo da una entrada de su grupo de recompensas. Cuánto aparece lo marca su rareza.
struct ChestType {
    int id = -1;
    std::string name;
    int rarity = 1;   // id de la tabla rarity (la misma de los pokémon)
    int rewardId = -1; // grupo de la tabla reward
};
