#pragma once
#include <algorithm>
#include <array>
#include <numeric>
#include "../../../models/pokemon/pokemonSpecies.hpp"
#include "../../../utils/core/randomUtil.hpp"

// EVs de un pokémon y su clasificación de potencial (única definición de estas reglas).
// Un EV por estadística, en el orden de BaseStats: PS, ataque, ataque especial, defensa, defensa especial, velocidad.
namespace EvRules {
    using Evs = std::array<int, BaseStats::COUNT>;

    inline constexpr int MAX_EV = 31;
    inline constexpr int DECENT_TOTAL = 96; // suma mínima de EVs para ser "decente" (C) sin llegar a tener ninguno al máximo
    inline constexpr int TOP_STATS = 3;     // cuántas de las mejores estadísticas base cuentan para S+

    enum class Rank { D, C, B, A, S, S_PLUS };

    // EVs máximos con los que sale un pokémon (su potencial): un valor al azar entre 0 y MAX_EV en cada estadística.
    // Los EVs reales empiezan en 0 y solo suben entrenándolo, sin pasar de estos máximos.
    inline Evs rollCaps() {
        Evs caps;
        for (int& cap : caps) cap = RandomUtil::integer(0, MAX_EV);
        return caps;
    }

    // Estadísticas de más a menos base (a igualdad, por orden de índice).
    inline std::array<int, BaseStats::COUNT> byBase(const BaseStats& base) {
        std::array<int, BaseStats::COUNT> order;
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return base.at(a) > base.at(b); });
        return order;
    }

    // Máximos del pokémon inicial: sus 3 mejores estadísticas base (las de su evolución final) al máximo; el resto al azar.
    inline Evs starterCaps(const BaseStats& finalBase) {
        Evs caps = rollCaps();
        const auto order = byBase(finalBase);
        for (int i = 0; i < TOP_STATS; ++i) caps[order[i]] = MAX_EV;
        return caps;
    }

    // Potencial al capturar con un mínimo garantizado: tras tirar al azar, se ponen al máximo estadísticas al azar hasta
    // llegar al rango mínimo (B = 1 al máximo, A = 2, S = 3; S+ = sus tres mejores estadísticas base).
    inline Evs rollCaps(const BaseStats& base, Rank minimum) {
        Evs caps = rollCaps();
        if (minimum == Rank::S_PLUS) {
            const auto order = byBase(base);
            for (int i = 0; i < TOP_STATS; ++i) caps[order[i]] = MAX_EV;
            return caps;
        }
        const int needed = minimum == Rank::S ? 3 : minimum == Rank::A ? 2 : minimum == Rank::B ? 1 : 0;
        std::array<int, BaseStats::COUNT> order;
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), RandomUtil::engine());
        int maxed = static_cast<int>(std::count(caps.begin(), caps.end(), MAX_EV));
        for (int i = 0; i < BaseStats::COUNT && maxed < needed; ++i) {
            if (caps[order[i]] < MAX_EV) { caps[order[i]] = MAX_EV; ++maxed; }
        }
        return caps;
    }

    inline Evs clamped(Evs evs) {
        for (int& ev : evs) ev = (std::clamp)(ev, 0, MAX_EV);
        return evs;
    }

    // Potencial según los EVs máximos (caps) de un pokémon: D = EVs muy malos; C = decentes; B = al menos un EV al máximo; A = dos; S = tres o más;
    // S+ = los EVs de sus tres mejores estadísticas base están al máximo.
    inline Rank rank(const BaseStats& base, const Evs& evs) {
        const auto order = byBase(base);
        bool topMaxed = true;
        for (int i = 0; i < TOP_STATS; ++i) topMaxed = topMaxed && evs[order[i]] >= MAX_EV;
        if (topMaxed) return Rank::S_PLUS;

        const int maxed = static_cast<int>(std::count_if(evs.begin(), evs.end(), [](int ev) { return ev >= MAX_EV; }));
        if (maxed >= 3) return Rank::S;
        if (maxed == 2) return Rank::A;
        if (maxed == 1) return Rank::B;
        return std::accumulate(evs.begin(), evs.end(), 0) >= DECENT_TOTAL ? Rank::C : Rank::D;
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
