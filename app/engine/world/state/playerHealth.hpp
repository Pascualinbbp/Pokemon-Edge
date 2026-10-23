#pragma once
#include <algorithm>
#include "../rules/healthRules.hpp"

// Vida del jugador: el daño la reduce, y tras un rato sin recibirlo se cura sola.
class PlayerHealth {
    public:
    float value() const { return m_health; }
    float fraction() const { return m_health / HealthRules::MAX_HEALTH; }
    bool full() const { return m_health >= HealthRules::MAX_HEALTH; }
    bool dead() const { return m_health <= 0.0f; }
    // 0..1 mientras dura el destello de dolor.
    float hurtFlash() const { return (std::clamp)(m_flash / HealthRules::HURT_FLASH, 0.0f, 1.0f); }

    void hurt(float amount) {
        if (amount <= 0.0f || dead()) return;
        m_health = (std::max)(0.0f, m_health - amount);
        m_quiet = 0.0f;
        if (amount >= 1.0f) m_flash = HealthRules::HURT_FLASH;
    }
    // Quemadura: la prende la lava; hace daño mientras dura y se apaga con agua o deslizándose.
    bool burning() const { return m_burn > 0.0f; }
    void ignite() { m_burn = HealthRules::BURN_DURATION; }
    void extinguish() { m_burn = 0.0f; }
    // Cura sin cortar la espera de la regeneración natural.
    void heal(float amount) { if (!dead()) m_health = (std::min)(HealthRules::MAX_HEALTH, m_health + amount); }
    void update(float dt) {
        if (m_burn > 0.0f) { m_burn = (std::max)(0.0f, m_burn - dt); hurt(HealthRules::BURN_PER_SECOND * dt); }
        m_flash = (std::max)(0.0f, m_flash - dt);
        m_quiet += dt;
        if (!dead() && m_quiet >= HealthRules::REGEN_DELAY) m_health = (std::min)(HealthRules::MAX_HEALTH, m_health + HealthRules::REGEN_PER_SECOND * dt);
    }
    void restore() { m_health = HealthRules::MAX_HEALTH; m_quiet = 0.0f; m_flash = 0.0f; m_burn = 0.0f; }

    private:
    float m_health = HealthRules::MAX_HEALTH;
    float m_quiet = 0.0f;
    float m_flash = 0.0f;
    float m_burn = 0.0f; // segundos de quemadura que quedan
};
