#pragma once
#include <algorithm>
#include <array>
#include <numeric>
#include "../../models/pokemonSpecies.hpp"
#include "../../utils/core/randomUtil.hpp"

// EVs de un pokémon y su clasificación de potencial (única definición de estas reglas).
// Un EV por estadística, en el orden de BaseStats: PS, ataque, ataque especial, defensa, defensa especial, velocidad.
namespace EvRules {
    using Evs = std::array<int, BaseStats::COUNT>;

    inline constexpr int MAX_EV = 31;
    inline constexpr int DECENT_TOTAL = 96; // suma mínima de EVs para ser "decente" (C) sin llegar a tener ninguno al máximo
    inline constexpr int TOP_STATS = 3;     // cuántas de las mejores estadísticas base cuentan para S+

    enum class Rank { D, C, B, A, S, S_PLUS };

    // EVs al capturar: un valor al azar entre 0 y MAX_EV en cada estadística.
    inline Evs roll() {
        Evs evs;
        for (int& ev : evs) ev = RandomUtil::integer(0, MAX_EV);
        return evs;
    }

    inline Evs clamped(Evs evs) {
        for (int& ev : evs) ev = (std::clamp)(ev, 0, MAX_EV);
        return evs;
    }

    // Potencial: D = EVs muy malos; C = decentes; B = al menos un EV al máximo; A = dos; S = tres o más;
    // S+ = los EVs de sus tres mejores estadísticas base están al máximo.
    inline Rank rank(const BaseStats& base, const Evs& evs) {
        std::array<int, BaseStats::COUNT> order;
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return base.at(a) > base.at(b); });

        bool topMaxed = true;
        for (int i = 0; i < TOP_STATS; ++i) topMaxed = topMaxed && evs[order[i]] >= MAX_EV;
        if (topMaxed) return Rank::S_PLUS;

        const int maxed = static_cast<int>(std::count_if(evs.begin(), evs.end(), [](int ev) { return ev >= MAX_EV; }));
        if (maxed >= 3) return Rank::S;
        if (maxed == 2) return Rank::A;
        if (maxed == 1) return Rank::B;
        return std::accumulate(evs.begin(), evs.end(), 0) >= DECENT_TOTAL ? Rank::C : Rank::D;
    }

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
