#pragma once
#include <DirectXMath.h>
#include "pokemonStyle.hpp"

// Color de cada material (por id de la base de datos). Es el único sitio donde se define: lo usan el objeto suelto
// del mundo y los iconos de la interfaz.
namespace ItemStyle {
    inline DirectX::XMFLOAT3 materialColor(int materialId) {
        switch (materialId) {
            case 1:  return { 0.55f, 0.36f, 0.18f }; // Madera
            case 2:  return { 0.60f, 0.60f, 0.63f }; // Piedra
            case 3:  return { 0.85f, 0.55f, 0.35f }; // Hierro
            case 4:  return { 0.16f, 0.16f, 0.18f }; // Carbón
            case 5:  return { 0.45f, 0.90f, 0.98f }; // Diamante
            case 6:  return { 0.85f, 0.20f, 0.28f }; // Baya
            case 7:  return { 0.98f, 0.80f, 0.20f }; // Baya dorada
            default: { // minerales evolutivos (id = id del tipo + 6): el color del tipo
                int r, g, b;
                PokemonStyle::typeRgb(materialId - 6, r, g, b);
                return { r / 255.0f, g / 255.0f, b / 255.0f };
            }
        }
    }
}
