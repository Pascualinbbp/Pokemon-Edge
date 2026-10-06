#pragma once
#include <DirectXMath.h>

// Aspecto de cada pokémon y tipo (por id de la base de datos). Es el único sitio donde se definen sus colores.
namespace PokemonStyle {
    // Color del cuerpo del pokémon en el mundo (el orden de pokemon.sql fija los id).
    inline DirectX::XMFLOAT3 color(int speciesId) {
        switch (speciesId) {
            case 1:  return { 0.30f, 0.68f, 0.58f }; // Bulbasaur
            case 2:  return { 0.27f, 0.60f, 0.50f }; // Ivysaur
            case 3:  return { 0.22f, 0.52f, 0.46f }; // Venusaur
            case 4:  return { 0.95f, 0.48f, 0.20f }; // Charmander
            case 5:  return { 0.82f, 0.28f, 0.20f }; // Charmeleon
            case 6:  return { 0.95f, 0.55f, 0.15f }; // Charizard
            case 7:  return { 0.35f, 0.65f, 0.90f }; // Squirtle
            case 8:  return { 0.28f, 0.50f, 0.85f }; // Wartortle
            case 9:  return { 0.22f, 0.40f, 0.75f }; // Blastoise
            default: return { 1.00f, 0.55f, 0.10f };
        }
    }

    // Color de la etiqueta de cada tipo elemental (RGB 0..255).
    inline void elementRgb(int elementId, int& r, int& g, int& b) {
        switch (elementId) {
            case 1:  r = 120; g = 200; b = 80;  break; // Planta
            case 2:  r = 160; g = 90;  b = 200; break; // Veneno
            case 3:  r = 240; g = 128; b = 48;  break; // Fuego
            case 4:  r = 104; g = 144; b = 240; break; // Agua
            case 5:  r = 168; g = 144; b = 240; break; // Volador
            default: r = 160; g = 160; b = 170; break;
        }
    }
}
