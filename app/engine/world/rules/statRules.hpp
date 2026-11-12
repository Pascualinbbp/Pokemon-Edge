#pragma once
#include <algorithm>
#include <array>
#include "../../../models/pokemon/nature.hpp"
#include "../../../models/pokemon/pokemonSpecies.hpp"

// Estadísticas reales de un pokémon (única definición de la fórmula de la serie). Todos los decimales se descartan a la baja.
//   PS      = ((2·base + IV + EV/4) · nivel) / 100 + nivel + 10
//   resto   = (((2·base + IV + EV/4) · nivel) / 100 + 5) · naturaleza   (x1,1 la que sube, x0,9 la que baja)
// IV: 0..31 (ver IvRules) · EV: puntos repartidos por el jugador, 0..MAX_EV por estadística y TOTAL_EV en total.
namespace StatRules {
    using Evs = std::array<int, BaseStats::COUNT>;

    inline constexpr int MAX_EV = 252;   // por estadística
    inline constexpr int TOTAL_EV = 510; // entre todas

    inline int total(const Evs& evs) { int sum = 0; for (const int ev : evs) sum += ev; return sum; }
    inline int remaining(const Evs& evs) { return (std::max)(0, TOTAL_EV - total(evs)); }

    // EVs válidos: cada uno entre 0 y MAX_EV y la suma sin pasar de TOTAL_EV (si se pasa, se recorta desde la última estadística).
    inline Evs clamped(Evs evs) {
        for (int& ev : evs) ev = (std::clamp)(ev, 0, MAX_EV);
        for (int i = BaseStats::COUNT - 1; i >= 0 && total(evs) > TOTAL_EV; --i) evs[static_cast<size_t>(i)] = (std::max)(0, evs[static_cast<size_t>(i)] - (total(evs) - TOTAL_EV));
        return evs;
    }

    // Cambia el EV de una estadística hasta 'wanted' respetando los dos límites. Devuelve el valor que queda.
    inline int setEv(Evs& evs, int stat, int wanted) {
        if (stat < 0 || stat >= BaseStats::COUNT) return 0;
        const int others = total(evs) - evs[static_cast<size_t>(stat)];
        evs[static_cast<size_t>(stat)] = (std::clamp)(wanted, 0, (std::min)(MAX_EV, (std::max)(0, TOTAL_EV - others)));
        return evs[static_cast<size_t>(stat)];
    }

    // Multiplicador en porcentaje de la naturaleza para una estadística (110, 100 o 90; los PS no cambian).
    inline int naturePercent(const Nature* nature, int stat) {
        if (!nature || nature->neutral() || stat == 0) return 100;
        return stat == nature->increased ? 110 : stat == nature->decreased ? 90 : 100;
    }

    inline int hp(int base, int level, int iv, int ev) { return ((2 * base + iv + ev / 4) * level) / 100 + level + 10; }

    inline int stat(int base, int level, int iv, int ev, int percent = 100) { return ((((2 * base + iv + ev / 4) * level) / 100 + 5) * percent) / 100; }

    // Estadística 'index' (orden de BaseStats) de una especie con esos IV, EV y naturaleza.
    inline int of(const BaseStats& base, int index, int level, int iv, int ev, const Nature* nature) {
        return index == 0 ? hp(base.at(0), level, iv, ev) : stat(base.at(index), level, iv, ev, naturePercent(nature, index));
    }
}
