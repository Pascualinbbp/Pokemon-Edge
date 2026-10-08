#pragma once
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include "../../physics/physicsWorld.hpp"
#include "../../../models/gameData.hpp"

// Mapa de hábitats de una partida: se genera al empezar (según la semilla) con zonas circulares de cada hábitat. Cada
// hábitat sale al menos una vez, con un número de zonas y un tamaño al azar dentro de lo que dice la base de datos.
// Entre zonas no hay corte: en cada punto cada hábitat tiene un peso (suman 1) que baja suavemente al alejarse de su
// zona, y donde se juntan varios se mezclan (ecotonos). Todo el suelo es transitable, así que todos son accesibles.
class HabitatMap {
    public:
    static constexpr float MARGIN = 6.0f;      // las zonas no se centran pegadas al borde del mundo
    static constexpr float FALLOFF = 2.2f;     // cuanto mayor, más corta la transición entre zonas
    static constexpr float SPACING = 0.55f;     // separación mínima entre centros, en fracción de la suma de radios
    static constexpr int PLACE_TRIES = 40;

    struct Zone {
        int habitat = 0; // índice en GameData::habitats
        float x = 0.0f;
        float z = 0.0f;
        float radius = 10.0f;
        float stretch = 1.0f; // alargamiento de la zona (elipse) y hacia dónde apunta
        float cosA = 1.0f;
        float sinA = 0.0f;
    };

    void generate(const GameData& data, unsigned seed) {
        m_seed = seed;
        m_habitats = static_cast<int>(data.habitats.size());
        m_zones.clear();
        std::mt19937 random(seed);
        const auto between = [&](float low, float high) { return low + (high - low) * std::uniform_real_distribution<float>(0.0f, 1.0f)(random); };

        const float reach = Physics::World::HALF_SIZE - MARGIN;
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& habitat = data.habitats[h];
            const int count = std::uniform_int_distribution<int>((std::max)(1, habitat.minZones), (std::max)(1, habitat.maxZones))(random);
            for (int i = 0; i < count; ++i) {
                Zone zone;
                zone.habitat = h;
                zone.radius = between(habitat.minRadius, habitat.maxRadius);
                zone.stretch = between(0.65f, 1.55f);
                const float angle = between(0.0f, 6.2831853f);
                zone.cosA = std::cos(angle);
                zone.sinA = std::sin(angle);
                for (int attempt = 0; attempt < PLACE_TRIES; ++attempt) {
                    zone.x = between(-reach, reach);
                    zone.z = between(-reach, reach);
                    if (separated(zone)) break;
                }
                m_zones.push_back(zone);
            }
        }
        // Ondulación de las fronteras: el mismo desplazamiento en todo el mundo, distinto en cada semilla.
        for (float& value : m_warp) value = between(0.0f, 6.2831853f);
        m_warpSize = between(3.0f, 6.0f);
        m_warpFrequency = between(0.07f, 0.13f);
    }

    unsigned seed() const { return m_seed; }
    const std::vector<Zone>& zones() const { return m_zones; }
    int habitatCount() const { return m_habitats; }

    // Peso de cada hábitat en (x, z): suman 1. Vacío si no hay hábitats.
    void weights(float x, float z, std::vector<float>& out) const {
        const float wx = x + m_warpSize * std::sin(z * m_warpFrequency + m_warp[0]) + 0.5f * m_warpSize * std::sin(z * m_warpFrequency * 2.3f + m_warp[2]);
        const float wz = z + m_warpSize * std::sin(x * m_warpFrequency + m_warp[1]) + 0.5f * m_warpSize * std::sin(x * m_warpFrequency * 2.3f + m_warp[3]);
        x = wx;
        z = wz;
        out.assign(m_habitats, 0.0f);
        if (m_zones.empty()) return;

        float nearest = 1.0e9f;
        for (const Zone& zone : m_zones) nearest = (std::min)(nearest, distance2(zone, x, z));
        float total = 0.0f;
        for (const Zone& zone : m_zones) {
            const float w = std::exp(-FALLOFF * (distance2(zone, x, z) - nearest));
            out[zone.habitat] += w;
            total += w;
        }
        for (float& w : out) w /= total;
    }

    // Índice del hábitat que más pesa en (x, z) (-1 si no hay hábitats).
    int dominant(float x, float z) const {
        std::vector<float> w;
        weights(x, z, w);
        return w.empty() ? -1 : static_cast<int>(std::max_element(w.begin(), w.end()) - w.begin());
    }

    private:
    // Distancia al cuadrado al centro, medida en radios de la zona (las grandes pesan más lejos).
    static float distance2(const Zone& zone, float x, float z) {
        const float dx = x - zone.x, dz = z - zone.z;
        const float u = (dx * zone.cosA + dz * zone.sinA) / (zone.radius * zone.stretch);
        const float v = (dz * zone.cosA - dx * zone.sinA) * zone.stretch / zone.radius;
        return u * u + v * v;
    }

    bool separated(const Zone& candidate) const {
        for (const Zone& other : m_zones) {
            const float dx = candidate.x - other.x, dz = candidate.z - other.z;
            const float min = (candidate.radius + other.radius) * SPACING;
            if (dx * dx + dz * dz < min * min) return false;
        }
        return true;
    }

    unsigned m_seed = 0;
    float m_warp[4] = {};
    float m_warpSize = 0.0f;
    float m_warpFrequency = 0.1f;
    int m_habitats = 0;
    std::vector<Zone> m_zones;
};
