#pragma once
#include <algorithm>
#include <cmath>
#include "pokemonRules.hpp"

// Experiencia de combate: UNA sola fórmula para todo (salvajes, entrenadores, combate por turnos, capturas).
//  - Depende de la diferencia de nivel: un rival más fuerte que tu pokémon da más; uno más débil, menos.
//  - Se reparte entre los pokémon que participaron y siguen en pie al derrotarlo (un debilitado no cobra y no cuenta en el reparto).
//  - La rareza de la especie rival la multiplica.
//  - Capturar en lugar de derrotar da la misma experiencia reducida (CAPTURE_FACTOR).
// El jugador gana experiencia propia al derrotar a un pokémon de un entrenador (PLAYER_FACTOR de la de un pokémon).
namespace ExperienceRules {
    enum class Source { DEFEAT, CAPTURE };
    inline constexpr float CAPTURE_FACTOR = 0.5f;
    inline constexpr float PLAYER_FACTOR = 0.25f;

    // >1 si el rival tiene más nivel que quien gana la experiencia, <1 si tiene menos (fórmula de la serie).
    inline float levelFactor(int foeLevel, int ownLevel) {
        const float foe = static_cast<float>(foeLevel), own = static_cast<float>(ownLevel);
        return std::pow((2.0f * foe + 10.0f) / (foe + own + 10.0f), 2.5f);
    }

    inline int pokemonXp(int foeLevel, int ownLevel, int participants, float rarity = 1.0f, Source source = Source::DEFEAT) {
        float xp = static_cast<float>(PokemonRules::battleXp(foeLevel)) * levelFactor(foeLevel, ownLevel) * rarity / static_cast<float>((std::max)(1, participants));
        if (source == Source::CAPTURE) xp *= CAPTURE_FACTOR;
        return (std::max)(1, static_cast<int>(std::lround(xp)));
    }

    inline int playerXp(int foeLevel, int playerLevel) {
        return (std::max)(1, static_cast<int>(std::lround(static_cast<float>(pokemonXp(foeLevel, playerLevel, 1)) * PLAYER_FACTOR)));
    }
}
