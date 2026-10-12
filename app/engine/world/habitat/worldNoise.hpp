#pragma once
#include <cmath>
#include <cstdint>

// Ruido suave (valor interpolado sobre una malla de hashes) para dar forma natural al mundo: costas, relieve, cuevas.
// Determinista: la misma semilla da siempre el mismo ruido.
class WorldNoise {
    public:
    explicit WorldNoise(unsigned seed = 1) : m_seed(seed * 2654435761u + 0x9E3779B9u) {}

    // Ruido de valor en [-1, 1].
    float value(float x, float z) const {
        const float fx = std::floor(x), fz = std::floor(z);
        const int ix = static_cast<int>(fx), iz = static_cast<int>(fz);
        const float tx = fade(x - fx), tz = fade(z - fz);
        const float a = lattice(ix, iz), b = lattice(ix + 1, iz), c = lattice(ix, iz + 1), d = lattice(ix + 1, iz + 1);
        return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * tz;
    }

    // Suma de octavas (cada una el doble de fina y 'gain' veces más débil), normalizada a [-1, 1].
    float fbm(float x, float z, int octaves = 4, float gain = 0.5f) const {
        float sum = 0.0f, amplitude = 1.0f, total = 0.0f, frequency = 1.0f;
        for (int i = 0; i < octaves; ++i) {
            sum += amplitude * value(x * frequency + 17.3f * i, z * frequency - 9.1f * i);
            total += amplitude;
            amplitude *= gain;
            frequency *= 2.0f;
        }
        return sum / total;
    }

    // Crestas: 1 en las líneas donde el ruido cruza el cero y baja hacia los lados (cordilleras, túneles).
    float ridge(float x, float z, int octaves = 4) const {
        float sum = 0.0f, amplitude = 1.0f, total = 0.0f, frequency = 1.0f;
        for (int i = 0; i < octaves; ++i) {
            const float n = 1.0f - std::fabs(value(x * frequency + 5.7f * i, z * frequency + 3.3f * i));
            sum += amplitude * n * n;
            total += amplitude;
            amplitude *= 0.5f;
            frequency *= 2.0f;
        }
        return sum / total;
    }

    private:
    static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    float lattice(int x, int z) const {
        uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(z) * 668265263u + m_seed;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x7FFFFF) - 1.0f;
    }

    uint32_t m_seed;
};
