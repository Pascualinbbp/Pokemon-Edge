#pragma once
#include <string>

// Hito de la Pokédex (tabla dex_reward): se gana por especie al llegar a cierto logro con ella y da experiencia al jugador y progreso de la especie.
struct DexReward {
    // CAPTURES: pokémon de la especie capturados · SHINY: variocolor capturados · RANK: mejor rango analizado (índice de
    // EvRules::RANKS: 0 = D ... 4 = S, 5 = S+) · LEVEL: nivel más alto alcanzado por uno de la especie.
    enum class Kind { CAPTURES, SHINY, RANK, LEVEL };

    int id = -1;
    Kind kind = Kind::CAPTURES;
    int threshold = 1;
    std::string description;
    int playerXp = 0;   // experiencia del jugador (antes de multiplicarla por la rareza de la especie)
    int progress = 1;   // puntos de progreso de la especie
    int speciesId = -1; // -1 = vale para todas las especies

    static Kind parse(const std::string& text) {
        return text == "SHINY" ? Kind::SHINY : text == "RANK" ? Kind::RANK : text == "LEVEL" ? Kind::LEVEL : Kind::CAPTURES;
    }
    bool appliesTo(int species) const { return speciesId < 0 || speciesId == species; }
};
