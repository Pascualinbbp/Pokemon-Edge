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

    // true con la probabilidad indicada (0..100).
    inline bool roll(float percent) {
        return range(0.0f, 100.0f) < percent;
    }
}
