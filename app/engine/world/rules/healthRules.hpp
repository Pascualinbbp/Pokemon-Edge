#pragma once
#include <algorithm>
#include <cmath>

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

    inline constexpr float WATER_CUSHION = 5.0f;       // m de caída que amortigua cada metro de agua profunda
    inline constexpr float CLOUD_BOUNCE = 0.55f;       // fracción de la velocidad de impacto que devuelve el suelo de nube
    inline constexpr float CLOUD_BOUNCE_MIN = 6.0f;    // m/s de impacto a partir de los cuales rebota

    // Velocidad de impacto que queda tras caer en agua de la profundidad dada (0 si la profundidad absorbe toda la caída).
    inline float impactInWater(float impactSpeed, float depth) {
        const float height = fallHeight(impactSpeed) - WATER_CUSHION * depth;
        return height <= 0.0f ? 0.0f : std::sqrt(2.0f * GRAVITY * height);
    }

    inline float fallDamage(float impactSpeed) {
        return (std::max)(0.0f, fallHeight(impactSpeed) - SAFE_FALL) * FALL_DAMAGE_PER_M;
    }
}
