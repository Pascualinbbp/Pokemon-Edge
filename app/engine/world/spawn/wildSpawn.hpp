#pragma once
#include <algorithm>
#include <numeric>
#include <vector>
#include "../habitat/habitatMap.hpp"
#include "../habitat/weatherSystem.hpp"
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Qué pokémon salvaje aparece en un punto: peso base de cada pokémon en los hábitats de ahí (mezclados según su peso en
// el punto) y, en las fronteras, el de sus ecotonos; multiplicado por sus condiciones favorables (clima, día o noche).
// Con la base de datos sin hábitats, todos aparecen por igual.
namespace WildSpawn {
    inline constexpr float ECOTONE_MIN_SHARE = 0.2f; // presencia mínima de cada hábitat para que cuente su ecotono

    struct Context {
        std::vector<float> habitats;                // pesos de HabitatMap::weights
        std::vector<WeatherSystem::Local> weather;  // climas del punto
        bool night = false;
    };

    inline float strengthOf(const Context& context, int weatherId) {
        for (const WeatherSystem::Local& local : context.weather) if (local.weatherId == weatherId) return local.strength;
        return 0.0f;
    }

    // Peso base de la especie en el punto (0 = no aparece aquí).
    inline float baseWeight(const GameData& data, const PokemonSpecies& species, const Context& context) {
        const auto presence = [&](int habitatId) {
            const int index = data.habitatIndex(habitatId);
            return index >= 0 && index < static_cast<int>(context.habitats.size()) ? context.habitats[index] : 0.0f;
        };
        float weight = 0.0f;
        for (const HabitatWeight& entry : species.spawn.habitats) weight += presence(entry.habitatId) * entry.weight;
        for (const EcotoneWeight& entry : species.spawn.ecotones) {
            const float a = presence(entry.habitatA), b = presence(entry.habitatB);
            if (a >= ECOTONE_MIN_SHARE && b >= ECOTONE_MIN_SHARE) weight += (std::min)(a, b) * 2.0f * entry.weight;
        }
        return weight;
    }

    // Multiplicador de las condiciones favorables: cada clima actúa en proporción a su fuerza.
    inline float conditionFactor(const PokemonSpecies& species, const Context& context) {
        float factor = 1.0f;
        for (const SpawnCondition& condition : species.spawn.conditions) {
            float presence = 0.0f;
            switch (condition.kind) {
                case SpawnCondition::Kind::WEATHER: presence = (std::min)(1.0f, strengthOf(context, condition.weatherId)); break;
                case SpawnCondition::Kind::DAY:     presence = context.night ? 0.0f : 1.0f; break;
                case SpawnCondition::Kind::NIGHT:   presence = context.night ? 1.0f : 0.0f; break;
            }
            factor *= 1.0f + (condition.multiplier - 1.0f) * presence;
        }
        return factor;
    }

    // Índice en GameData::species del pokémon que aparece (-1 si no hay ninguno).
    inline int pick(const GameData& data, const Context& context) {
        std::vector<float> weights(data.species.size(), 0.0f);
        float total = 0.0f;
        for (size_t i = 0; i < data.species.size(); ++i) {
            const float base = context.habitats.empty() ? 1.0f : baseWeight(data, data.species[i], context);
            weights[i] = base * conditionFactor(data.species[i], context);
            total += weights[i];
        }
        if (total <= 0.0f) { // las condiciones lo anulan todo: se ignoran
            for (size_t i = 0; i < data.species.size(); ++i) weights[i] = context.habitats.empty() ? 1.0f : baseWeight(data, data.species[i], context);
        }
        const float sum = std::accumulate(weights.begin(), weights.end(), 0.0f);
        if (sum <= 0.0f) return data.species.empty() ? -1 : 0;
        float pickAt = RandomUtil::range(0.0f, sum);
        for (size_t i = 0; i < weights.size(); ++i) {
            if (weights[i] <= 0.0f) continue;
            pickAt -= weights[i];
            if (pickAt < 0.0f) return static_cast<int>(i);
        }
        for (size_t i = weights.size(); i-- > 0;) if (weights[i] > 0.0f) return static_cast<int>(i);
        return data.species.empty() ? -1 : 0;
    }
}
