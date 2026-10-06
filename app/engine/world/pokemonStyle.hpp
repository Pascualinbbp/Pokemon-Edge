#pragma once
#include <DirectXMath.h>

// Aspecto de cada especie (por id de la base de datos). Es el único sitio donde se define su color.
namespace PokemonStyle {
    inline DirectX::XMFLOAT3 color(int speciesId) {
        switch (speciesId) {
            case 1:  return { 0.98f, 0.82f, 0.15f }; // Pikachu
            case 2:  return { 0.55f, 0.62f, 0.80f }; // Machop
            case 3:  return { 0.35f, 0.75f, 0.30f }; // Scyther
            case 4:  return { 0.52f, 0.48f, 0.44f }; // Geodude
            case 5:  return { 0.30f, 0.68f, 0.58f }; // Bulbasaur
            default: return { 1.00f, 0.55f, 0.10f };
        }
    }
}
