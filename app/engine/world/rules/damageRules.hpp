#pragma once
#include <algorithm>
#include <cmath>
#include "../../../models/gameData.hpp"

// Cálculo de daño entre pokémon (única definición, para cualquier pokémon que ataque). Un pokémon solo ataca con sus propios tipos
// (los de la tabla pokemon); contra el defensor elige el que más daño le hace según la tabla de tipos (type_matchup), contando los dos
// tipos del defensor y las habilidades que potencian un tipo con poca vida. Pega de cerca si su ataque supera al ataque especial y de
// lejos en caso contrario; de cerca cuentan ataque y defensa, de lejos ataque especial y defensa especial.
namespace DamageRules {
    inline constexpr float BASE_DAMAGE = 0.2f;   // fracción de vida que quita un golpe entre pokémon igualados
    inline constexpr float MIN_DAMAGE = 0.05f;   // antes de aplicar tipos
    inline constexpr float MAX_DAMAGE = 0.6f;
    inline constexpr float MAX_HIT = 1.0f;       // un golpe nunca quita más que toda la vida

    enum class Style { MELEE, RANGED };

    inline Style styleOf(const PokemonSpecies& species) { return species.stats.spAttack > species.stats.attack ? Style::RANGED : Style::MELEE; }

    struct Hit {
        float fraction = 0.0f;      // fracción de la vida del defensor que quita (0 = no le afecta)
        int typeId = -1;            // tipo con el que ataca
        float effectiveness = 1.0f; // multiplicador de la tabla de tipos (0, 0.5, 1, 2, 4...)
        Style style = Style::MELEE;
        bool lands() const { return fraction > 0.0f; }
    };

    // Potenciación por habilidad de un tipo de ataque (1 si ninguna de las suyas lo potencia con esa vida).
    inline float abilityBoost(const GameData& data, const PokemonSpecies& attacker, int typeId, float hpFraction) {
        float boost = 1.0f;
        for (const int id : { attacker.abilityId, attacker.passiveId }) {
            const Ability* ability = id >= 0 ? data.ability(id) : nullptr;
            if (ability && ability->effect == Ability::Effect::TYPE_BOOST && ability->typeId == typeId && hpFraction < ability->hpBelow) boost *= ability->multiplier;
        }
        return boost;
    }

    // Daño de un golpe del atacante al defensor con el tipo que más le conviene. hpFraction = vida actual del atacante (1 = completa).
    inline Hit compute(const GameData& data, const PokemonSpecies& attacker, int attackerLevel, const PokemonSpecies& defender, int defenderLevel,
                       float hpFraction = 1.0f) {
        Hit hit;
        hit.style = styleOf(attacker);
        float best = -1.0f;
        for (const int typeId : attacker.types) {
            const float value = data.typeChart.multiplier(typeId, defender.types) * abilityBoost(data, attacker, typeId, hpFraction);
            if (value > best) { // con igualdad se queda el primero (el principal)
                best = value;
                hit.typeId = typeId;
            }
        }
        hit.effectiveness = best < 0.0f ? 1.0f : best;
        if (hit.effectiveness <= TypeChart::IMMUNE) return hit;

        const bool ranged = hit.style == Style::RANGED;
        const int attack = ranged ? attacker.stats.spAttack : attacker.stats.attack;
        const int defense = ranged ? defender.stats.spDefense : defender.stats.defense;
        const float ratio = static_cast<float>(attackerLevel * attack) / static_cast<float>((std::max)(1, defenderLevel * defense));
        hit.fraction = (std::min)((std::clamp)(BASE_DAMAGE * ratio, MIN_DAMAGE, MAX_DAMAGE) * hit.effectiveness, MAX_HIT);
        return hit;
    }
}
