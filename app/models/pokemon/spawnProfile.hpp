#pragma once
#include <vector>

// Dónde y con qué condiciones aparece un pokémon salvaje (tablas pokemon_habitat, pokemon_ecotone y pokemon_condition).
struct HabitatWeight {
    int habitatId = -1;
    float weight = 0.0f;
};

struct EcotoneWeight {
    int habitatA = -1;
    int habitatB = -1;
    float weight = 0.0f;
};

// Condición favorable: multiplica su peso de aparición con cierto clima, de día o de noche.
struct SpawnCondition {
    enum class Kind { WEATHER, DAY, NIGHT };
    Kind kind = Kind::WEATHER;
    int weatherId = -1;
    float multiplier = 1.0f;
};

struct SpawnProfile {
    std::vector<HabitatWeight> habitats;
    std::vector<EcotoneWeight> ecotones;
    std::vector<SpawnCondition> conditions;
};
