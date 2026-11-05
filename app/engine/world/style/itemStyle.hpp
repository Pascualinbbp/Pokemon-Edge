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

    // Modelo 3D (assets/models/<nombre>.glb) y sprite (assets/hud/item/<nombre>.png) de una poción según los PS que cura: poción 20, superpoción 60,
    // ultrapoción 120 (y las de curación total). Lo usan el icono de la mochila y el efecto sobre el acompañante.
    inline const char* medicineModel(int amount) {
        return amount <= 20 ? "potions/potion" : amount <= 60 ? "potions/superpotion" : "potions/ultrapotion";
    }

    inline constexpr float MEDICINE_HELD_HEIGHT = 0.34f; // alto del frasco en la mano (m)
    inline constexpr float MEDICINE_FRONT_YAW = 0.0f;    // giro para que la boquilla (+z en el modelo) apunte hacia delante; se ajusta aquí si hiciera falta

    // Color del spray de cada poción.
    inline DirectX::XMFLOAT3 medicineColor(int amount) {
        return amount <= 20 ? DirectX::XMFLOAT3{ 0.65f, 0.45f, 0.95f } : amount <= 60 ? DirectX::XMFLOAT3{ 0.95f, 0.40f, 0.70f } : DirectX::XMFLOAT3{ 1.0f, 0.60f, 0.25f };
    }
}
