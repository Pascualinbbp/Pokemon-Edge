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
    inline constexpr float LAVA_PER_SECOND = 45.0f;    // dentro de lava
    inline constexpr float BURN_PER_SECOND = 7.0f;     // quemado: daño mientras dura la quemadura
    inline constexpr float BURN_DURATION = 9.0f;       // segundos que arde tras salir de la lava (el agua o deslizarse en el suelo la apagan)
    inline constexpr float LAVA_HEAT = 70.0f;
    inline constexpr float SPRING_HEAT = 15.0f;        // °C que suma estar dentro de una charca termal
    inline constexpr float MIN_TEMPERATURE = -30.0f;   // la altura no enfría por debajo de esto          // °C que suma estar pegado a la lava
    inline constexpr float SPRING_HEAL_PER_SECOND = 6.0f;   // aguas termales: curación mientras el temporizador no ha vencido
    inline constexpr float SPRING_SAFE_TIME = 30.0f;        // segundos en una misma charca antes de que el agua haga daño
    inline constexpr float SPRING_HURT_PER_SECOND = 5.0f;   // daño por pasarse de tiempo en la charca
    inline constexpr float REGEN_DELAY = 6.0f;         // segundos sin daño hasta empezar a curarse
    inline constexpr float REGEN_PER_SECOND = 2.5f;
    inline constexpr float HEAL_FLASH = 0.8f;          // segundos que dura el brillo verde al curarse
    inline constexpr float HURT_MIN_POWER = 0.3f;      // intensidad del destello con un daño mínimo (el progresivo lo mantiene encendido)
    inline constexpr float HURT_FULL_AT = 25.0f;       // daño de un golpe que da el destello al máximo
    inline constexpr float HURT_FLASH = 0.6f;          // segundos que dura el destello rojo de la pantalla

    // Altura de caída libre equivalente a una velocidad de impacto (v² = 2·g·h).
    inline float fallHeight(float impactSpeed) { return impactSpeed * impactSpeed / (2.0f * GRAVITY); }

    inline constexpr float WATER_CUSHION = 5.0f;       // m de caída que amortigua cada metro de agua profunda
    inline constexpr float CLOUD_BOUNCE = 0.72f;       // fracción de la velocidad de impacto que devuelve el suelo de nube
    inline constexpr float CLOUD_BOUNCE_MIN = 3.0f;    // m/s de impacto a partir de los cuales rebota

    // Velocidad de impacto que queda tras caer en agua de la profundidad dada (0 si la profundidad absorbe toda la caída).
    inline float impactInWater(float impactSpeed, float depth) {
        const float height = fallHeight(impactSpeed) - WATER_CUSHION * depth;
        return height <= 0.0f ? 0.0f : std::sqrt(2.0f * GRAVITY * height);
    }

    inline float fallDamage(float impactSpeed) {
        return (std::max)(0.0f, fallHeight(impactSpeed) - SAFE_FALL) * FALL_DAMAGE_PER_M;
    }
}
