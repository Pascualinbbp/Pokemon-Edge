#pragma once
#include <string>

// Objeto de entrenamiento o medicina (fila de item con efecto LEVEL, IV, IV_CAP, IV_MAX, HEAL o REVIVE): modifica a un pokémon del jugador.
struct TrainingItem {
    enum class Effect { LEVEL, IV, IV_CAP, IV_MAX, HEAL, REVIVE }; // IV: vitamina (sube IV) · IV_CAP: chapa plateada (un tope de IV al máximo) · IV_MAX: chapa dorada (todos)

    int id = -1; // id en la tabla item
    std::string name;
    std::string description;
    Effect effect = Effect::LEVEL;
    std::string asset; // "items/<categoría>/<id>": modelo 3D y sprite (assets/models/ y assets/sprites/)
    int rgb = -1;      // color del chorro al usarlo (0xRRGGBB, color del efecto)
    int amount = 1;    // niveles, puntos de IV, PS que cura o % de PS con el que revive, según el efecto
    bool percent = false; // HEAL: amount es un % de los PS máximos (la Max poción cura el 100 %)
};
