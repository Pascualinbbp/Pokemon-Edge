#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "styleRegistry.hpp"

// Aspecto de cada pokémon (por id de la base de datos); el color de los tipos viene de la tabla type.
namespace PokemonStyle {
    // Color propio de cada especie sin color a mano: el tono recorre el círculo cromático con el id (ángulo áureo), así que dos
    // especies seguidas nunca se parecen.
    inline DirectX::XMFLOAT3 hue(int speciesId) {
        const float h = std::fmod(static_cast<float>(speciesId) * 0.61803399f, 1.0f) * 6.0f;
        const float s = 0.55f, v = 0.88f;
        const float f = h - std::floor(h);
        const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
        switch (static_cast<int>(h)) {
            case 0:  return { v, t, p };
            case 1:  return { q, v, p };
            case 2:  return { p, v, t };
            case 3:  return { p, q, v };
            case 4:  return { t, p, v };
            default: return { v, p, q };
        }
    }

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
            default: return hue(speciesId);
        }
    }

    // Color del variocolor: los canales rotados y algo más claros.
    inline DirectX::XMFLOAT3 shiny(const DirectX::XMFLOAT3& c) {
        return { (std::min)(1.0f, c.z * 1.3f + 0.15f), (std::min)(1.0f, c.x * 1.1f + 0.1f), (std::min)(1.0f, c.y * 1.3f + 0.15f) };
    }

    // Color de la etiqueta de cada tipo (RGB 0..255), de la columna color de la tabla type.
    inline void typeRgb(int typeId, int& r, int& g, int& b) {
        const DirectX::XMFLOAT3 c = StyleRegistry::lookup(StyleRegistry::typeColors(), typeId, { 0.66f, 0.66f, 0.47f });
        r = static_cast<int>(c.x * 255.0f + 0.5f); g = static_cast<int>(c.y * 255.0f + 0.5f); b = static_cast<int>(c.z * 255.0f + 0.5f);
    }
}
