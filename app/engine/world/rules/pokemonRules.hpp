#pragma once
#include "../../../models/pokemon/pokemonSpecies.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Reglas de los pokémon individuales (nivel y variocolor).
namespace PokemonRules {
    inline constexpr float SHINY_CHANCE = 0.2f; // % de pokémon salvajes variocolor
    inline constexpr int STARTER_LEVEL = 5;

    inline bool rollShiny() { return RandomUtil::roll(SHINY_CHANCE); }

    // Nivel con el que aparece un pokémon salvaje de la especie.
    inline int rollLevel(const PokemonSpecies& species) { return RandomUtil::integer(species.minLevel, species.maxLevel); }
}
