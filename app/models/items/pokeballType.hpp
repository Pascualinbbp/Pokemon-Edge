#pragma once
#include <cstdio>
#include <string>

// Pokéball: una fila de item cuyo efecto es CAPTURE (o CAPTURE_EXCLUSIVE, la bola del pokémon inicial, que no es un objeto).
struct PokeballType {
    int id = -1;
    std::string name;
    std::string description;
    float captureMultiplier = 1.0f; // multiplica el porcentaje de captura
    float shinyMultiplier = 1.0f;   // ídem sobre los pokémon shiny
    float multiplier(bool shiny) const { return shiny ? shinyMultiplier : captureMultiplier; }
    // "x1.5", o "x0.2 · Shiny x3.0" si la bola trata distinto a los shiny.
    std::string ratio() const {
        char text[64];
        if (shinyMultiplier == captureMultiplier) std::snprintf(text, sizeof(text), "x%.1f", captureMultiplier);
        else std::snprintf(text, sizeof(text), "x%.2f  |  Shiny x%.1f", captureMultiplier, shinyMultiplier);
        return text;
    }
    bool obtainable = true;         // false = bola exclusiva (la del pokémon inicial): no es un objeto
    std::string asset;              // "items/pokeballs/<id>": modelo 3D y sprite (assets/models/ y assets/sprites/)
};
