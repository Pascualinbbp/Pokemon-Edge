#pragma once
#include <cmath>
#include "../../../models/pokemon/pokemonSpecies.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Reglas de los pokémon individuales (nivel y variocolor).
namespace PokemonRules {
    inline constexpr float STARTER_SHINY_CHANCE = 0.1f; // % de que el inicial elegido sea variocolor (mínimo)
    inline constexpr float SHINY_CHANCE = 0.2f; // % de pokémon salvajes variocolor
    inline constexpr int STARTER_LEVEL = 5;

    inline constexpr float XP_BASE = 25.0f;      // experiencia de un pokémon para subir de nivel: XP_BASE * nivel ^ XP_EXPONENT
    inline constexpr float XP_EXPONENT = 1.3f;
    inline constexpr int BATTLE_XP_PER_LEVEL = 10; // experiencia por derrotar a un salvaje: BATTLE_XP_BASE + esto * su nivel
    inline constexpr int BATTLE_XP_BASE = 20;

    inline int xpToNext(int level) { return static_cast<int>(XP_BASE * std::pow(static_cast<float>(level), XP_EXPONENT)); }
    inline int battleXp(int wildLevel) { return BATTLE_XP_BASE + BATTLE_XP_PER_LEVEL * wildLevel; }

    // PS máximos según la fórmula de la serie (sin EVs ni IVs: base, nivel). Los decimales se descartan siempre a la baja.
    inline int maxHp(const PokemonSpecies& species, int level) { return (2 * species.stats.hp * level) / 100 + level + 10; }

    inline bool rollShiny() { return RandomUtil::roll(SHINY_CHANCE); }

    // Nivel con el que aparece un pokémon salvaje de la especie.
    inline int rollLevel(const PokemonSpecies& species) { return RandomUtil::integer(species.minLevel, species.maxLevel); }
}
