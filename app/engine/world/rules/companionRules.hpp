#pragma once
#include <algorithm>

// Reglas del pokémon que acompaña al jugador (única definición): sus modos de comportamiento y su combate.
namespace CompanionRules {
    // Qué hace el acompañante: recolecta materiales de alrededor, ayuda a capturar debilitando a los salvajes (y deja de
    // atacar cuando están bajos de vida) o los combate hasta derrotarlos para ganar experiencia.
    enum class Mode { COLLECT, CAPTURE, BATTLE };

    inline constexpr Mode MODES[] = { Mode::COLLECT, Mode::CAPTURE, Mode::BATTLE };

    inline const char* label(Mode mode) {
        switch (mode) {
            case Mode::COLLECT: return "Recolección";
            case Mode::CAPTURE: return "Captura";
            case Mode::BATTLE:  return "Combate";
        }
        return "?";
    }

    inline Mode next(Mode mode) { return MODES[(static_cast<int>(mode) + 1) % static_cast<int>(std::size(MODES))]; }

    inline constexpr float ATTACK_RANGE = 2.4f;      // distancia al centro del salvaje para golpearlo
    inline constexpr float CAPTURE_STOP_HP = 0.3f;   // en modo captura deja de atacar por debajo de esta fracción de vida
    inline constexpr float BASE_DAMAGE = 0.2f;       // fracción de vida que quita un golpe entre pokémon igualados
    inline constexpr float MIN_DAMAGE = 0.05f;
    inline constexpr float MAX_DAMAGE = 0.6f;

    // Fracción de la vida del salvaje que quita un golpe, según nivel y ataque del acompañante frente a nivel y defensa del salvaje.
    inline float damageFraction(int attackerLevel, int attack, int defenderLevel, int defense) {
        const float ratio = static_cast<float>(attackerLevel * attack) / static_cast<float>((std::max)(1, defenderLevel * defense));
        return (std::clamp)(BASE_DAMAGE * ratio, MIN_DAMAGE, MAX_DAMAGE);
    }
}
