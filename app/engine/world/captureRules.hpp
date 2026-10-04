#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

// Reglas del porcentaje de captura: bonificaciones fijas sobre el porcentaje base del pokémon.
namespace CaptureRules {
    inline constexpr float BACK_MULTIPLIER = 1.15f;    // el jugador está por detrás del pokémon
    inline constexpr float STEALTH_MULTIPLIER = 1.25f; // el jugador va agachado: el pokémon no lo ha detectado
    inline constexpr float MAX_PERCENT = 99.0f;
    inline constexpr float BEHIND_DOT = -0.25f;       // por debajo de este valor se considera "por la espalda"
    inline constexpr float MIN_BASE = 5.0f;           // rango del porcentaje base aleatorio (pruebas)
    inline constexpr float MAX_BASE = 95.0f;

    // ¿Está el jugador detrás del pokémon (según hacia dónde mira, targetYaw: 0 = +Z)?
    inline bool isBehind(const DirectX::XMFLOAT3& targetPos, float targetYaw, const DirectX::XMFLOAT3& playerPos) {
        const float dx = playerPos.x - targetPos.x;
        const float dz = playerPos.z - targetPos.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length < 1.0e-4f) return false;

        float s, c;
        DirectX::XMScalarSinCos(&s, &c, targetYaw);
        return (s * dx + c * dz) / length < BEHIND_DOT;
    }

    inline float percent(float base, bool behind, bool hidden) {
        float value = base;
        if (behind) value *= BACK_MULTIPLIER;
        if (hidden) value *= STEALTH_MULTIPLIER;
        return (std::min)(value, MAX_PERCENT);
    }
}
