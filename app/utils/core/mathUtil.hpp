#pragma once
#include <algorithm>

// Funciones matemáticas pequeñas compartidas.
namespace MathUtil {
    // Curva suave 0..1 -> 0..1 (arranca y termina despacio). Recorta la entrada a [0, 1].
    inline float smoothstep(float x) {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }
}
