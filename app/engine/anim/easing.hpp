#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

// Curvas de animación genéricas (0..1 -> 0..1) y utilidades de interpolación. Sirven para cualquier animación del juego.
namespace Easing {
    enum class Curve { LINEAR, IN_QUAD, OUT_QUAD, IN_CUBIC, OUT_CUBIC, IN_OUT, OUT_BACK };

    inline float clamp01(float x) { return (std::clamp)(x, 0.0f, 1.0f); }

    inline float apply(Curve curve, float t) {
        t = clamp01(t);
        switch (curve) {
            case Curve::IN_QUAD:   return t * t;
            case Curve::OUT_QUAD:  return 1.0f - (1.0f - t) * (1.0f - t);
            case Curve::IN_CUBIC:  return t * t * t;
            case Curve::OUT_CUBIC: { const float u = 1.0f - t; return 1.0f - u * u * u; }
            case Curve::IN_OUT:    return t * t * (3.0f - 2.0f * t);
            case Curve::OUT_BACK: {
                constexpr float C = 1.70158f;
                const float u = t - 1.0f;
                return 1.0f + (C + 1.0f) * u * u * u + C * u * u;
            }
            default:               return t;
        }
    }

    // Avance (0..1) de 't' dentro del tramo [from, to].
    inline float remap(float t, float from, float to) { return to > from ? clamp01((t - from) / (to - from)) : (t >= to ? 1.0f : 0.0f); }

    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

    // Pulso 0 -> 1 -> 0 (por ejemplo el brillo de un destello).
    inline float pulse(float t) { return std::sin(3.14159265f * clamp01(t)); }

    // Interpola dos ángulos por el camino corto.
    inline float lerpAngle(float a, float b, float t) { return a + DirectX::XMScalarModAngle(b - a) * t; }

    // Gira 'current' hacia 'target' como mucho 'maxStep' radianes, por el camino corto.
    inline float approachAngle(float current, float target, float maxStep) {
        return current + (std::clamp)(DirectX::XMScalarModAngle(target - current), -maxStep, maxStep);
    }
}
