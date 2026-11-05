#pragma once
#include <string>

// Objeto de entrenamiento (tabla training_item): modifica a un pokémon del jugador desde el menú de pokémon.
struct TrainingItem {
    enum class Effect { LEVEL, EV, HEAL, REVIVE };

    int id = -1;
    std::string name;
    std::string description;
    Effect effect = Effect::LEVEL;
    std::string model; // modelo 3D y sprite (assets/models/<model>.glb, assets/hud/item/<model>.png); vacío si no tiene
    int rgb = -1;      // color del chorro al usarlo (0xRRGGBB)
    int amount = 1; // niveles, puntos de EV, PS que cura o % de PS con el que revive, según el efecto
};
