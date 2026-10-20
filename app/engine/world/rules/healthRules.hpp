#pragma once
#include <algorithm>

// Salud del jugador: cuánto daño causa cada cosa. Es el único sitio donde se definen estas cifras.
namespace HealthRules {
    inline constexpr float MAX_HEALTH = 100.0f;
    inline constexpr float GRAVITY = 20.0f;            // el de Physics::World (la caída se calcula con la misma física)
    inline constexpr float SAFE_FALL = 4.0f;           // m de caída libre que no hacen daño
    inline constexpr float FALL_DAMAGE_PER_M = 5.0f;   // puntos de vida por cada metro de caída por encima del seguro (muerte a unos 24 m)
    inline constexpr float DROWN_PER_SECOND = 14.0f;   // sin aire bajo el agua
    inline constexpr float REGEN_DELAY = 6.0f;         // segundos sin daño hasta empezar a curarse
    inline constexpr float REGEN_PER_SECOND = 2.5f;
    inline constexpr float HURT_FLASH = 0.6f;          // segundos que dura el destello rojo de la pantalla

    // Altura de caída libre equivalente a una velocidad de impacto (v² = 2·g·h).
    inline float fallHeight(float impactSpeed) { return impactSpeed * impactSpeed / (2.0f * GRAVITY); }

    inline float fallDamage(float impactSpeed) {
        return (std::max)(0.0f, fallHeight(impactSpeed) - SAFE_FALL) * FALL_DAMAGE_PER_M;
    }
}
