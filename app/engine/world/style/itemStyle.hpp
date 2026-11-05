#pragma once
#include <DirectXMath.h>
#include "styleRegistry.hpp"

// Colores y medidas de los objetos. El color de cada material viene de la columna color de la tabla material.
namespace ItemStyle {
    inline DirectX::XMFLOAT3 materialColor(int materialId) {
        return StyleRegistry::lookup(StyleRegistry::materialColors(), materialId, { 0.60f, 0.60f, 0.63f });
    }

    inline constexpr float MEDICINE_HELD_HEIGHT = 0.34f; // alto del frasco (o cristal) en la mano (m)
    inline constexpr float MEDICINE_FRONT_YAW = 0.0f;    // giro para que la boquilla (+z en el modelo) apunte hacia delante
}
