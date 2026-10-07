#pragma once
#include <algorithm>
#include <cmath>
#include "../../../models/gameData.hpp"

// Recompensa de la máquina de investigación por cada pokémon capturado: depende de su rareza (lo poco que aparece
// la especie, tabla pokemon), su nivel y si es variocolor.
namespace ResearchRules {
    inline constexpr int BASE_REWARD = 40;
    inline constexpr int REWARD_PER_LEVEL = 4;
    inline constexpr float SHINY_MULTIPLIER = 10.0f;

    // 1 para la especie más común; crece con la raíz de lo rara que es.
    inline float rarity(const GameData& data, const PokemonSpecies& species) {
        float common = species.spawnWeight;
        for (const PokemonSpecies& entry : data.species) common = (std::max)(common, entry.spawnWeight);
        return species.spawnWeight > 0.0f ? std::sqrt(common / species.spawnWeight) : 1.0f;
    }

    inline int reward(const GameData& data, const PokemonSpecies& species, int level, bool shiny) {
        const float value = static_cast<float>(BASE_REWARD + REWARD_PER_LEVEL * level) * rarity(data, species) * (shiny ? SHINY_MULTIPLIER : 1.0f);
        return static_cast<int>(std::lround(value));
    }
}
