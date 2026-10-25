#pragma once
#include <string>

// Objeto de entrenamiento (tabla training_item): modifica a un pokémon del jugador desde el menú de pokémon.
struct TrainingItem {
    enum class Effect { LEVEL, EV, HEAL, REVIVE };

    int id = -1;
    std::string name;
    std::string description;
    Effect effect = Effect::LEVEL;
    int amount = 1; // niveles, puntos de EV, PS que cura o % de PS con el que revive, según el efecto
};
