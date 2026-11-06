#pragma once
#include <string>

// Objeto de entrenamiento o medicina (fila de item con efecto LEVEL, EV, HEAL o REVIVE): modifica a un pokémon del jugador.
struct TrainingItem {
    enum class Effect { LEVEL, EV, HEAL, REVIVE };

    int id = -1; // id en la tabla item
    std::string name;
    std::string description;
    Effect effect = Effect::LEVEL;
    std::string asset; // "items/<categoría>/<id>": modelo 3D y sprite (assets/models/ y assets/sprites/)
    int rgb = -1;      // color del chorro al usarlo (0xRRGGBB, color del efecto)
    int amount = 1;    // niveles, puntos de EV, PS que cura o % de PS con el que revive, según el efecto
};
