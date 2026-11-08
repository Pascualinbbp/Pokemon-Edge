#pragma once
#include <string>

// Objeto equipable (fila de item con efecto HOLD_*): el pokémon lo lleva consigo y tiene efecto en combate o con el tiempo.
struct HeldItem {
    enum class Effect { EXP_SHARE, QUICK_CLAW, RAZOR_CLAW, FOCUS_BAND, LEFTOVERS, LIFE_ORB };

    int id = -1; // id en la tabla item
    std::string name;
    std::string description;
    Effect effect = Effect::EXP_SHARE;
    int amount = 0; // %: de la experiencia que reparte, de probabilidad, de PS que recupera o de daño extra, según el efecto
    int rgb = -1;   // color del icono (0xRRGGBB, color del efecto)
};
