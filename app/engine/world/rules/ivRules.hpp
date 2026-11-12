#pragma once
#include <algorithm>
#include <array>
#include <numeric>
#include "../../../models/pokemon/pokemonSpecies.hpp"
#include "../../../utils/core/randomUtil.hpp"

// IVs de un pokémon y su clasificación de potencial (única definición de estas reglas).
// Un IV por estadística, en el orden de BaseStats: PS, ataque, ataque especial, defensa, defensa especial, velocidad.
// Los IV son innatos: el valor (0..MAX_IV) empieza en 0 y sube con vitaminas hasta su tope (ivCaps); el tope es el potencial del pokémon.
// Una estadística es "maxeable" cuando su tope vale MAX_IV; el rango depende de cuántas hay (los IV no cambian al evolucionar).
namespace IvRules {
    using Ivs = std::array<int, BaseStats::COUNT>;

    inline constexpr int MAX_IV = 31;

    enum class Rank { D, C, B, A, S, S_PLUS };

    // Topes con los que sale un pokémon: un valor al azar entre 0 y MAX_IV en cada estadística.
    inline Ivs rollCaps() {
        Ivs caps;
        for (int& cap : caps) cap = RandomUtil::integer(0, MAX_IV);
        return caps;
    }

    inline int maxedCount(const Ivs& caps) { return static_cast<int>(std::count_if(caps.begin(), caps.end(), [](int cap) { return cap >= MAX_IV; })); }

    // Cuántas estadísticas maxeables exige cada rango como mínimo (D = ninguna, C = 1, B = 2, A = 3 o 4, S = 5, S+ = todas).
    inline int neededFor(Rank rank) {
        switch (rank) {
            case Rank::S_PLUS: return BaseStats::COUNT;
            case Rank::S:      return BaseStats::COUNT - 1;
            case Rank::A:      return 3;
            case Rank::B:      return 2;
            case Rank::C:      return 1;
            default:           return 0;
        }
    }

    // Máximos del pokémon inicial: todos sus IV pueden llegar al máximo (S+).
    inline Ivs starterCaps() {
        Ivs caps;
        caps.fill(MAX_IV);
        return caps;
    }

    // Potencial al capturar con un mínimo garantizado: tras tirar al azar, se ponen al máximo estadísticas al azar hasta llegar al rango mínimo.
    inline Ivs rollCaps(Rank minimum) {
        Ivs caps = rollCaps();
        const int needed = neededFor(minimum);
        std::array<int, BaseStats::COUNT> order;
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), RandomUtil::engine());
        int maxed = maxedCount(caps);
        for (int i = 0; i < BaseStats::COUNT && maxed < needed; ++i) {
            if (caps[order[i]] < MAX_IV) { caps[order[i]] = MAX_IV; ++maxed; }
        }
        return caps;
    }

    inline Ivs clamped(Ivs ivs) {
        for (int& iv : ivs) iv = (std::clamp)(iv, 0, MAX_IV);
        return ivs;
    }

    // Potencial según los topes de IV: D = ninguno al máximo; C = uno; B = dos; A = tres o cuatro; S = cinco; S+ = los seis.
    inline Rank rank(const Ivs& caps) {
        const int maxed = maxedCount(caps);
        if (maxed >= BaseStats::COUNT) return Rank::S_PLUS;
        if (maxed == BaseStats::COUNT - 1) return Rank::S;
        if (maxed >= 3) return Rank::A;
        if (maxed == 2) return Rank::B;
        if (maxed == 1) return Rank::C;
        return Rank::D;
    }

    // Chapa plateada: el tope de una estadística pasa al máximo (el valor del IV no cambia). Devuelve false si ya lo estaba.
    inline bool raiseCap(Ivs& caps, int stat) {
        if (stat < 0 || stat >= BaseStats::COUNT || caps[static_cast<size_t>(stat)] >= MAX_IV) return false;
        caps[static_cast<size_t>(stat)] = MAX_IV;
        return true;
    }

    // Chapa dorada: todos los topes al máximo (S+). Devuelve false si ya lo estaban.
    inline bool raiseAllCaps(Ivs& caps) {
        if (maxedCount(caps) >= BaseStats::COUNT) return false;
        caps.fill(MAX_IV);
        return true;
    }

    // Rangos de menor a mayor potencial, para recorrerlos en la interfaz.
    inline constexpr Rank RANKS[] = { Rank::D, Rank::C, Rank::B, Rank::A, Rank::S, Rank::S_PLUS };

    inline const char* label(Rank rank) {
        switch (rank) {
            case Rank::D:      return "D";
            case Rank::C:      return "C";
            case Rank::B:      return "B";
            case Rank::A:      return "A";
            case Rank::S:      return "S";
            case Rank::S_PLUS: return "S+";
        }
        return "?";
    }
}
