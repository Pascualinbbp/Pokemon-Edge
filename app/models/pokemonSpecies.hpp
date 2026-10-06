#pragma once
#include <string>
#include <vector>

// Habilidad de trabajo de una especie, con su multiplicador de velocidad.
struct SpeciesSkill {
    int skillId = -1;
    float power = 1.0f;
};

// Especie de pokémon (tablas pokemon_species y species_skill).
struct PokemonSpecies {
    int id = -1;
    std::string name;
    float spawnWeight = 1.0f;
    std::vector<SpeciesSkill> skills;

    const SpeciesSkill* skill(int skillId) const {
        for (const SpeciesSkill& entry : skills) if (entry.skillId == skillId) return &entry;
        return nullptr;
    }
};
