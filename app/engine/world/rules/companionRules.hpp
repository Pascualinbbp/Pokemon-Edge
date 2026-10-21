#pragma once
#include <algorithm>

// Reglas del pokémon que acompaña al jugador (única definición): sus modos de comportamiento y su alcance. El daño es DamageRules.
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

    inline constexpr float ATTACK_RANGE = 2.4f;      // distancia al centro del salvaje para golpearlo de cerca (ataque > ataque especial)
    inline constexpr float RANGED_RANGE = 8.0f;      // y de lejos (ataque especial > ataque)
    inline constexpr float RANGED_APPROACH = 0.75f;  // fracción del alcance a la que se detiene un atacante a distancia
    inline constexpr float WEAK_PROMPT_TIME = 8.0f;  // segundos que se ofrece capturar al debilitado antes de retirar el aviso
    inline constexpr float WEAK_REACH = 14.0f;       // distancia a la que se puede capturar al salvaje debilitado
    inline constexpr float CAPTURE_STOP_HP = 0.3f;   // en modo captura deja de atacar por debajo de esta fracción de vida
}
