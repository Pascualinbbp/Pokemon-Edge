#pragma once
#include <string>

// Objeto de entrenamiento (tabla training_item): modifica a un pokémon del jugador desde el menú de pokémon.
struct TrainingItem {
    enum class Effect { LEVEL, EV };

    int id = -1;
    std::string name;
    std::string description;
    Effect effect = Effect::LEVEL;
    int amount = 1; // niveles o puntos de EV que da cada uso
};
