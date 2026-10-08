#pragma once
#include <chrono>
#include <random>

// Números aleatorios de la aplicación (un único generador, sembrado una vez).
namespace RandomUtil {
    inline std::mt19937& engine() {
        static std::mt19937 generator{ std::random_device{}() };
        return generator;
    }

    // Semilla nueva para un mundo: mezcla el reloj con el generador del sistema (en algunos compiladores random_device se repite).
    inline unsigned freshSeed() {
        const unsigned clock = static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const unsigned seed = (clock * 2654435761u) ^ std::random_device{}() ^ static_cast<unsigned>(engine()());
        return seed ? seed : 1u;
    }

    // Número real en [low, high).
    inline float range(float low, float high) {
        std::uniform_real_distribution<float> distribution(low, high);
        return distribution(engine());
    }

    // Entero en [low, high] (ambos incluidos).
    inline int integer(int low, int high) {
        std::uniform_int_distribution<int> distribution(low, high);
        return distribution(engine());
    }

    // Índice elegido al azar entre los elementos según su peso (weight(elemento) >= 0). -1 si no hay peso total.
    template <typename Container, typename Weight>
    inline int weightedIndex(const Container& items, Weight weight) {
        float total = 0.0f;
        for (const auto& item : items) total += weight(item);
        if (total <= 0.0f) return -1;

        float pick = range(0.0f, total);
        int index = 0;
        for (const auto& item : items) {
            pick -= weight(item);
            if (pick < 0.0f) return index;
            ++index;
        }
        return static_cast<int>(items.size()) - 1;
    }

    // Elemento elegido al azar según su peso (ver weightedIndex). nullptr si no hay peso total.
    template <typename Container, typename Weight>
    inline const typename Container::value_type* pick(const Container& items, Weight weight) {
        const int index = weightedIndex(items, weight);
        return index < 0 ? nullptr : &items[index];
    }

    // true con la probabilidad indicada (0..100).
    inline bool roll(float percent) {
        return range(0.0f, 100.0f) < percent;
    }
}
