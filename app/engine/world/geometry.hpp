#pragma once
#include <DirectXMath.h>

// Distancias horizontales (plano XZ) entre dos puntos del mundo: lo que casi toda la lógica de alcance, búsqueda y rango necesita.
// Siempre al cuadrado: comparar con rango² evita la raíz en bucles que recorren muchas entidades.
namespace Geometry {
    inline float distance2XZ(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        const float dx = a.x - b.x, dz = a.z - b.z;
        return dx * dx + dz * dz;
    }
    inline bool within(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float range) { return distance2XZ(a, b) <= range * range; }
}
