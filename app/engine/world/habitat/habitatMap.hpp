#pragma once
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include "../../physics/physicsWorld.hpp"
#include "../../../models/gameData.hpp"

// Forma del mundo de una partida: se genera al empezar (según la semilla). El mundo es una isla irregular rodeada de mar.
// Cada hábitat sale al menos una vez, con zonas de tamaño y altura al azar dentro de lo que dice la base de datos; las
// playas se pegan a la costa, las orillas rodean a los ríos y lagos y el mar es lo que queda más allá de la costa, por
// profundidad. Entre zonas no hay corte: en cada punto cada hábitat tiene un peso (suman 1) que baja suavemente al alejarse
// de su zona, y la altura del terreno es la mezcla de las alturas de las zonas, así que el relieve pasa gradualmente de
// una a otra (montaña → pradera → playa → mar). Todo es analítico: el relieve se muestrea una vez en Physics::Heightfield.
class HabitatMap {
    public:
    static constexpr float MARGIN = 6.0f;       // las zonas no se centran pegadas al borde del mundo
    static constexpr float FALLOFF = 2.2f;      // cuanto mayor, más corta la transición entre zonas
    static constexpr float SPACING = 0.55f;     // separación mínima entre centros, en fracción de la suma de radios
    static constexpr int PLACE_TRIES = 60;
    static constexpr int LOBES = 4;             // ondulaciones del contorno de cada zona: lóbulos, entrantes, pliegues
    static constexpr float LOBE_AMPLITUDE[LOBES] = { 0.22f, 0.16f, 0.11f, 0.07f };
    static constexpr int LOBE_HARMONIC[LOBES] = { 2, 3, 5, 8 };

    static constexpr float ISLAND_RATIO = 0.7f;   // radio medio de la isla respecto a la mitad del mundo
    static constexpr float INLAND_MAX = 0.78f;    // las zonas normales se centran dentro de este t (t = 1 es la costa)
    static constexpr float COAST_T = 0.9f;        // t donde se centran las playas
    static constexpr float PLATEAU_FLAT = 22.0f;  // radio del terreno llano del inicio (m) y hasta dónde se funde con el resto
    static constexpr float PLATEAU_FADE = 36.0f;
    static constexpr int WAVES = 5;               // ondas que forman el relieve fino
    static constexpr int BANDS = 4;               // armónicos del contorno de la isla

    struct Zone {
        int habitat = 0; // índice en GameData::habitats
        float x = 0.0f;
        float z = 0.0f;
        float radius = 10.0f;
        float stretch = 1.0f; // alargamiento de la zona (elipse) y hacia dónde apunta
        float cosA = 1.0f;
        float sinA = 0.0f;
        float altitude = 3.0f; // altura de la zona (sorteada entre la mínima y la máxima de su hábitat)
        float roughness = 0.5f;
        float lobe[LOBES] = {}; // fase de cada ondulación del contorno (cada zona tiene la suya)
    };

    void generate(const GameData& data, unsigned seed) {
        m_seed = seed;
        m_habitats = static_cast<int>(data.habitats.size());
        m_zones.clear();
        m_seaMid.assign(m_habitats, 0.0f);
        m_seaWidth.assign(m_habitats, 1.0f);
        std::mt19937 random(seed);
        const auto between = [&](float low, float high) { return low + (high - low) * std::uniform_real_distribution<float>(0.0f, 1.0f)(random); };

        // Contorno de la isla (distinto en cada semilla) y ondas del relieve fino.
        for (int k = 0; k < BANDS; ++k) m_coastPhase[k] = between(0.0f, 6.2831853f);
        for (int k = 0; k < WAVES; ++k) {
            const float angle = between(0.0f, 6.2831853f);
            m_wave[k] = { std::cos(angle), std::sin(angle), between(0.05f, 0.14f) * std::pow(2.1f, static_cast<float>(k)), between(0.0f, 6.2831853f) };
        }
        m_islandRadius = ISLAND_RATIO * Physics::World::HALF_SIZE;
        for (float& value : m_warp) value = between(0.0f, 6.2831853f);
        m_warpSize = between(5.0f, 9.0f);
        m_warpFrequency = between(0.06f, 0.11f);

        const float reach = Physics::World::HALF_SIZE - MARGIN;
        const auto lobes = [&](Zone& zone) { for (float& phase : zone.lobe) phase = between(0.0f, 6.2831853f); };
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& habitat = data.habitats[h];
            if (habitat.sea()) {
                m_seaMid[h] = 0.5f * (habitat.minAltitude + habitat.maxAltitude);
                m_seaWidth[h] = (std::max)(0.5f * (habitat.maxAltitude - habitat.minAltitude), 0.2f);
                continue;
            }
            if (habitat.placement == HabitatPlacement::BESIDE) continue; // se colocan después, junto a su hábitat
            const int count = std::uniform_int_distribution<int>((std::max)(1, habitat.minZones), (std::max)(1, habitat.maxZones))(random);
            for (int i = 0; i < count; ++i) {
                Zone zone;
                zone.habitat = h;
                zone.radius = between(habitat.minRadius, habitat.maxRadius);
                zone.stretch = between(habitat.minStretch, (std::max)(habitat.minStretch, habitat.maxStretch));
                zone.altitude = between(habitat.minAltitude, habitat.maxAltitude);
                zone.roughness = habitat.roughness;
                lobes(zone);
                float angle = between(0.0f, 6.2831853f);
                for (int attempt = 0; attempt < PLACE_TRIES; ++attempt) {
                    if (habitat.placement == HabitatPlacement::COAST) { // sobre la costa, alargada a lo largo de ella
                        const float theta = between(0.0f, 6.2831853f);
                        const float r = COAST_T * coastRadius(theta);
                        zone.x = std::cos(theta) * r;
                        zone.z = std::sin(theta) * r;
                        angle = theta + 1.5707963f;
                    } else {
                        zone.x = between(-reach, reach);
                        zone.z = between(-reach, reach);
                        if (islandT(zone.x, zone.z) > INLAND_MAX) continue;
                        if (zone.altitude < 0.6f && zone.x * zone.x + zone.z * zone.z < PLATEAU_FADE * PLATEAU_FADE) continue; // el inicio no se moja
                    }
                    if (separated(zone)) break;
                }
                zone.cosA = std::cos(angle);
                zone.sinA = std::sin(angle);
                m_zones.push_back(zone);
            }
        }
        // Orillas: una zona más grande que cada zona del hábitat al que rodean (misma forma y orientación).
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& habitat = data.habitats[h];
            if (habitat.placement != HabitatPlacement::BESIDE) continue;
            for (size_t i = 0, count = m_zones.size(); i < count; ++i) {
                if (data.habitats[m_zones[i].habitat].id != habitat.besideId) continue;
                Zone zone = m_zones[i];
                zone.habitat = h;
                zone.radius += between(habitat.minRadius, habitat.maxRadius);
                zone.altitude = between(habitat.minAltitude, habitat.maxAltitude);
                zone.roughness = habitat.roughness;
                lobes(zone);
                m_zones.push_back(zone);
            }
        }
    }

    unsigned seed() const { return m_seed; }
    const std::vector<Zone>& zones() const { return m_zones; }
    int habitatCount() const { return m_habitats; }

    // Altura del terreno en (x, z): negativa bajo el agua (el nivel del mar es 0).
    float height(float x, float z) const {
        float w[MAX_ZONES];
        return shape(x, z, w).height;
    }

    // Peso de cada hábitat en (x, z): suman 1. Vacío si no hay hábitats.
    void weights(float x, float z, std::vector<float>& out) const {
        out.assign(m_habitats, 0.0f);
        if (m_zones.empty()) return;
        float w[MAX_ZONES];
        const Shape s = shape(x, z, w);
        for (size_t i = 0; i < m_zones.size(); ++i) out[m_zones[i].habitat] += w[i] * (1.0f - s.sea);
        if (s.sea <= 0.0f) return;

        // Mar: reparto por profundidad entre los hábitats de mar (cada uno con su rango de alturas).
        float total = 0.0f;
        for (int h = 0; h < m_habitats; ++h) {
            if (m_seaWidth[h] <= 0.0f || m_seaMid[h] == 0.0f) continue;
            const float d = (s.height - m_seaMid[h]) / (0.45f * m_seaWidth[h] + 0.2f);
            out[h] = std::exp(-0.5f * d * d);
            total += out[h];
        }
        if (total < 1.0e-6f) { // fuera de todos los rangos: el más cercano
            int best = -1;
            for (int h = 0; h < m_habitats; ++h) if (m_seaMid[h] != 0.0f && (best < 0 || std::fabs(s.height - m_seaMid[h]) < std::fabs(s.height - m_seaMid[best]))) best = h;
            if (best >= 0) { out[best] = 1.0f; total = 1.0f; }
        }
        for (int h = 0; h < m_habitats; ++h) if (m_seaMid[h] != 0.0f) out[h] *= s.sea / (std::max)(total, 1.0e-6f);
    }

    // Índice del hábitat que más pesa en (x, z) (-1 si no hay hábitats).
    int dominant(float x, float z) const {
        std::vector<float> w;
        weights(x, z, w);
        return w.empty() ? -1 : static_cast<int>(std::max_element(w.begin(), w.end()) - w.begin());
    }

    private:
    static constexpr int MAX_ZONES = 96;

    struct Wave { float cosA, sinA, frequency, phase; };
    struct Shape {
        float height = 0.0f;
        float sea = 0.0f; // fracción de mar en el punto (0 = tierra)
    };

    static float smooth(float edge0, float edge1, float x) {
        const float t = (std::clamp)((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Radio de la costa en la dirección 'theta' (rad): la isla no es un círculo.
    float coastRadius(float theta) const {
        constexpr float AMPLITUDE[BANDS] = { 0.14f, 0.09f, 0.06f, 0.04f };
        constexpr int HARMONIC[BANDS] = { 2, 3, 5, 7 };
        float scale = 1.0f;
        for (int k = 0; k < BANDS; ++k) scale += AMPLITUDE[k] * std::sin(static_cast<float>(HARMONIC[k]) * theta + m_coastPhase[k]);
        return m_islandRadius * scale;
    }

    // Distancia a la costa relativa: 0 en el centro, 1 en la costa, más allá, mar.
    float islandT(float x, float z) const { return std::sqrt(x * x + z * z) / coastRadius(std::atan2(z, x)); }

    // Ondulación de las fronteras: el mismo desplazamiento en todo el mundo, distinto en cada semilla (tres octavas: curvas
    // amplias, pliegues medianos y dientes pequeños).
    void warped(float& x, float& z) const {
        const float f = m_warpFrequency;
        const float wx = x + m_warpSize * std::sin(z * f + m_warp[0]) + 0.5f * m_warpSize * std::sin(z * f * 2.3f + m_warp[2] + x * f * 0.7f) + 0.25f * m_warpSize * std::sin(z * f * 5.1f + m_warp[1]);
        const float wz = z + m_warpSize * std::sin(x * f + m_warp[1]) + 0.5f * m_warpSize * std::sin(x * f * 2.3f + m_warp[3] + z * f * 0.7f) + 0.25f * m_warpSize * std::sin(x * f * 5.1f + m_warp[0]);
        x = wx;
        z = wz;
    }

    // Relieve fino en -1..1 (suma de ondas de distinta escala).
    float detail(float x, float z) const {
        float sum = 0.0f, amplitude = 1.0f, total = 0.0f;
        for (const Wave& wave : m_wave) {
            sum += amplitude * std::sin((x * wave.cosA + z * wave.sinA) * wave.frequency + wave.phase);
            total += amplitude;
            amplitude *= 0.62f;
        }
        return sum / total;
    }

    // Pesos de las zonas en (x, z) (suman 1), altura y fracción de mar.
    Shape shape(float x, float z, float* w) const {
        const float ox = x, oz = z;
        warped(x, z);
        Shape out;
        const size_t count = (std::min)(m_zones.size(), static_cast<size_t>(MAX_ZONES));
        if (count == 0) return out;

        float nearest = 1.0e9f;
        for (size_t i = 0; i < count; ++i) nearest = (std::min)(nearest, distance2(m_zones[i], x, z));
        float total = 0.0f, altitude = 0.0f, roughness = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            w[i] = std::exp(-FALLOFF * (distance2(m_zones[i], x, z) - nearest));
            total += w[i];
        }
        for (size_t i = 0; i < count; ++i) {
            w[i] /= total;
            altitude += w[i] * m_zones[i].altitude;
            roughness += w[i] * m_zones[i].roughness;
        }

        float land = altitude + roughness * detail(ox, oz);
        const float start = 1.0f - smooth(PLATEAU_FLAT, PLATEAU_FADE, std::sqrt(ox * ox + oz * oz)); // inicio llano
        land += (Physics::World::START_HEIGHT - land) * start;

        const float t = islandT(x, z);
        const float toSea = smooth(0.92f, 1.04f, t);
        const float seaFloor = -1.0f - 15.0f * smooth(1.0f, 1.7f, t);
        out.height = land + (seaFloor - land) * toSea;
        out.sea = smooth(0.85f, 1.0f, t) * (1.0f - smooth(-1.2f, 0.3f, out.height)) * (1.0f - start);
        return out;
    }

    // Distancia al cuadrado al centro, medida en radios de la zona (las grandes pesan más lejos).
    static float distance2(const Zone& zone, float x, float z) {
        const float dx = x - zone.x, dz = z - zone.z;
        const float u = (dx * zone.cosA + dz * zone.sinA) / (zone.radius * zone.stretch);
        const float v = (dz * zone.cosA - dx * zone.sinA) * zone.stretch / zone.radius;
        float lobes = 1.0f; // el radio varía con el ángulo: el contorno deja de ser una elipse perfecta
        const float angle = std::atan2(v, u);
        for (int k = 0; k < LOBES; ++k) lobes += LOBE_AMPLITUDE[k] * std::sin(static_cast<float>(LOBE_HARMONIC[k]) * angle + zone.lobe[k]);
        return (u * u + v * v) / (lobes * lobes);
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
    int m_habitats = 0;
    std::vector<Zone> m_zones;
    std::vector<float> m_seaMid;   // mar: altura central de cada hábitat de mar (0 = no es de mar)
    std::vector<float> m_seaWidth;
    float m_coastPhase[BANDS] = {};
    Wave m_wave[WAVES] = {};
    float m_islandRadius = 60.0f;
    float m_warp[4] = {};
    float m_warpSize = 0.0f;
    float m_warpFrequency = 0.1f;
};
