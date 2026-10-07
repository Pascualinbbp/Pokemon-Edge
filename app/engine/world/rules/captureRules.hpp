#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "evRules.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Reglas del porcentaje de captura: bonificaciones fijas sobre el porcentaje base del pokémon.
namespace CaptureRules {
    inline constexpr float BACK_MULTIPLIER = 1.15f;    // el jugador está por detrás del pokémon
    inline constexpr float STEALTH_MULTIPLIER = 1.25f; // el jugador va agachado: el pokémon no lo ha detectado
    inline constexpr float MAX_PERCENT = 99.0f;
    inline constexpr float BEHIND_DOT = -0.25f;       // por debajo de este valor se considera "por la espalda"

    // Porcentaje base de la especie: su ratio de captura fijo (1..255) escalado, corregido por la diferencia de nivel
    // entre el pokémon salvaje y el jugador (cada nivel por encima del jugador resta, cada nivel por debajo suma).
    inline constexpr float RATE_SCALE = 2.5f;         // % = ratio / 255 * 100 * RATE_SCALE
    inline constexpr float LEVEL_STEP = 0.04f;        // variación del porcentaje por nivel de diferencia
    inline constexpr float MIN_LEVEL_FACTOR = 0.1f;
    inline constexpr float MAX_LEVEL_FACTOR = 1.5f;

    // Lanzamientos especiales: capturan siempre, con la animación de un solo giro y estrellas.
    inline constexpr float LUCKY_CHANCE = 2.0f;         // % de lanzamientos con suerte
    inline constexpr float SUPER_LUCKY_CHANCE = 0.5f;   // % de lanzamientos con super suerte
    inline constexpr int CAPTURE_WOBBLES = 3;           // giros de una captura normal
    inline constexpr int LUCKY_WOBBLES = 1;
    inline constexpr int MAX_FAIL_WOBBLES = 2;          // un fallo da 0, 1 o 2 giros

    enum class Throw { NORMAL, LUCKY, SUPER_LUCKY };

    // Potencial mínimo que garantiza cada tipo de lanzamiento al pokémon capturado.
    inline EvRules::Rank minimumRank(Throw kind) {
        switch (kind) {
            case Throw::SUPER_LUCKY: return EvRules::Rank::S;
            case Throw::LUCKY:       return EvRules::Rank::A;
            default:                 return EvRules::Rank::D;
        }
    }

    // Resultado de un lanzamiento, decidido al instante; la animación solo lo reproduce.
    struct Result {
        bool captured;
        int wobbles;
        Throw kind;
    };

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

    inline Result roll(float chancePercent) {
        const float luck = RandomUtil::range(0.0f, 100.0f);
        if (luck < SUPER_LUCKY_CHANCE) return { true, LUCKY_WOBBLES, Throw::SUPER_LUCKY };
        if (luck < SUPER_LUCKY_CHANCE + LUCKY_CHANCE) return { true, LUCKY_WOBBLES, Throw::LUCKY };
        if (RandomUtil::roll(chancePercent)) return { true, CAPTURE_WOBBLES, Throw::NORMAL };
        return { false, RandomUtil::integer(0, MAX_FAIL_WOBBLES), Throw::NORMAL };
    }

    inline float basePercent(int catchRate, int wildLevel, int playerLevel) {
        const float factor = (std::clamp)(1.0f - static_cast<float>(wildLevel - playerLevel) * LEVEL_STEP, MIN_LEVEL_FACTOR, MAX_LEVEL_FACTOR);
        return static_cast<float>(catchRate) / 255.0f * 100.0f * RATE_SCALE * factor;
    }

    inline float percent(float base, bool behind, bool hidden, float ballMultiplier = 1.0f) {
        float value = base * ballMultiplier;
        if (behind) value *= BACK_MULTIPLIER;
        if (hidden) value *= STEALTH_MULTIPLIER;
        return (std::min)(value, MAX_PERCENT);
    }
}
