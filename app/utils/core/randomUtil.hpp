#pragma once
#include <random>

// Números aleatorios de la aplicación (un único generador, sembrado una vez).
namespace RandomUtil {
    inline std::mt19937& engine() {
        static std::mt19937 generator{ std::random_device{}() };
        return generator;
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

    // true con la probabilidad indicada (0..100).
    inline bool roll(float percent) {
        return range(0.0f, 100.0f) < percent;
    }
}
