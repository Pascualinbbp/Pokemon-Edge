#pragma once
#include <string>
#include <vector>

// Habilidad de recolección de un pokémon con su nivel fijo (tabla pokemon_skill).
struct SpeciesSkill {
    int skillId = -1;
    int level = 1;
};

// Habilidad de combate que puede tener un pokémon (tabla pokemon_ability).
// Las pasivas las tiene siempre; de las activas elige una el jugador.
struct SpeciesAbility {
    int abilityId = -1;
    bool passive = false;
};

// Estadísticas base.
struct BaseStats {
    int hp = 0;
    int attack = 0;
    int spAttack = 0;
    int defense = 0;
    int spDefense = 0;
    int speed = 0;
};

// Pokémon (tabla pokemon y sus tablas de relación).
struct PokemonSpecies {
    int id = -1;
    std::string name;
    std::string description;
    BaseStats stats;
    float spawnWeight = 1.0f;
    std::vector<int> elements;              // uno o dos id de element, en orden
    std::vector<SpeciesAbility> abilities;  // todas las habilidades de combate
    std::vector<SpeciesSkill> skills;       // habilidades de recolección

    // Nivel de recolección de una habilidad (0 = no la tiene).
    int skillLevel(int skillId) const {
        for (const SpeciesSkill& entry : skills) if (entry.skillId == skillId) return entry.level;
        return 0;
    }

    // Habilidad activa por defecto: la primera que puede aprender (-1 si no tiene).
    int defaultActiveAbility() const {
        for (const SpeciesAbility& entry : abilities) if (!entry.passive) return entry.abilityId;
        return -1;
    }

    bool canLearn(int abilityId) const {
        for (const SpeciesAbility& entry : abilities) if (!entry.passive && entry.abilityId == abilityId) return true;
        return false;
    }

    bool hasPassive(int abilityId) const {
        for (const SpeciesAbility& entry : abilities) if (entry.passive && entry.abilityId == abilityId) return true;
        return false;
    }
};
