#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "../entities/captureTarget.hpp"
#include "../habitat/habitatMap.hpp"
#include "../../physics/physicsWorld.hpp"
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Pokémon salvajes del mundo, repartidos en chunks cuadrados. Un chunk se puebla la primera vez que el jugador se acerca
// (antes de que pueda verlo, para que nada aparezca de golpe) y sus pokémon se guardan donde nacieron: al alejarse el jugador
// dejan de dibujarse y simularse, y al volver a acercarse reaparecen creciendo poco a poco. Los capturados o derrotados los
// renueva el chunk pasado un tiempo, siempre fuera de la vista. Cuántos hay en un chunk lo decide su hábitat (tabla habitat).
class WildField {
    public:
    static constexpr float CHUNK = 16.0f;
    static constexpr int CHUNKS = static_cast<int>(2.0f * Physics::World::HALF_SIZE / CHUNK);
    static constexpr int MAX_PER_CHUNK = 12;       // capacidad fija de cada chunk (las direcciones de sus pokémon no cambian)
    static constexpr float POPULATE_RADIUS = 64.0f; // chunks que se pueblan alrededor del jugador (más que el radio de carga)
    static constexpr float LOAD_RADIUS = 40.0f;     // dentro de este radio un pokémon aparece
    static constexpr float UNLOAD_RADIUS = 48.0f;   // fuera de este, desaparece (el margen evita parpadeos)
    static constexpr float EDGE = 1.5f;             // separación de los pokémon al borde de su chunk

    // Elige especie, nivel y brillo de un pokémon recién colocado.
    template <typename Assign>
    void update(float dt, float playerX, float playerZ, const GameData& data, const HabitatMap& habitats, Assign assign) {
        if (m_chunks.empty()) m_chunks.resize(CHUNKS * CHUNKS);
        m_loaded.clear();

        for (int row = 0; row < CHUNKS; ++row) {
            for (int col = 0; col < CHUNKS; ++col) {
                Chunk& chunk = m_chunks[row * CHUNKS + col];
                const float distance = distanceTo(col, row, playerX, playerZ);
                if (!chunk.populated && distance <= POPULATE_RADIUS) populate(chunk, col, row, data, habitats, assign);
                if (chunk.dormant && distance > UNLOAD_RADIUS) continue;

                bool awake = false;
                for (CaptureTarget& wild : chunk.wilds) {
                    if (wild.gone()) {
                        if (wild.refillDue(dt) && distance > UNLOAD_RADIUS) {
                            const DirectX::XMFLOAT2 at = pointIn(col, row);
                            wild.place(at.x, at.y, RandomUtil::range(-3.14159265f, 3.14159265f));
                            assign(wild);
                        }
                        awake = true;
                        continue;
                    }
                    const float dx = wild.body.position.x - playerX, dz = wild.body.position.z - playerZ;
                    const float radius = wild.shown() ? UNLOAD_RADIUS : LOAD_RADIUS;
                    const bool want = wild.speciesIndex() >= 0 && (wild.capturing() || dx * dx + dz * dz <= radius * radius);
                    wild.fade(dt, want);
                    if (wild.shown()) {
                        m_loaded.push_back(&wild);
                        awake = true;
                    }
                }
                chunk.dormant = !awake;
            }
        }
    }

    // Los pokémon cargados ahora (cerca del jugador): los únicos que se dibujan, se simulan y se pueden capturar.
    const std::vector<CaptureTarget*>& loaded() const { return m_loaded; }

    private:
    struct Chunk {
        std::vector<CaptureTarget> wilds;
        bool populated = false;
        bool dormant = false; // sin nada cargado ni pendiente de renovar: se salta mientras el jugador esté lejos
    };

    static float distanceTo(int col, int row, float x, float z) {
        const float x0 = -Physics::World::HALF_SIZE + col * CHUNK, z0 = -Physics::World::HALF_SIZE + row * CHUNK;
        const float dx = x - (std::clamp)(x, x0, x0 + CHUNK), dz = z - (std::clamp)(z, z0, z0 + CHUNK);
        return std::sqrt(dx * dx + dz * dz);
    }

    static DirectX::XMFLOAT2 pointIn(int col, int row) {
        return { -Physics::World::HALF_SIZE + col * CHUNK + RandomUtil::range(EDGE, CHUNK - EDGE),
                 -Physics::World::HALF_SIZE + row * CHUNK + RandomUtil::range(EDGE, CHUNK - EDGE) };
    }

    // Cuántos pokémon caben en el chunk: lo que pide cada hábitat, según lo que pesa en el centro del chunk.
    static int countFor(int col, int row, const GameData& data, const HabitatMap& habitats) {
        const float cx = -Physics::World::HALF_SIZE + (col + 0.5f) * CHUNK, cz = -Physics::World::HALF_SIZE + (row + 0.5f) * CHUNK;
        std::vector<float> weights;
        habitats.weights(cx, cz, weights);
        float count = 0.0f;
        for (size_t h = 0; h < weights.size() && h < data.habitats.size(); ++h) {
            count += weights[h] * RandomUtil::range(static_cast<float>(data.habitats[h].minWild), static_cast<float>(data.habitats[h].maxWild) + 0.99f);
        }
        if (weights.empty()) count = RandomUtil::range(2.0f, 5.0f);
        return (std::clamp)(static_cast<int>(count), 0, MAX_PER_CHUNK);
    }

    template <typename Assign>
    static void populate(Chunk& chunk, int col, int row, const GameData& data, const HabitatMap& habitats, Assign& assign) {
        chunk.populated = true;
        const int count = countFor(col, row, data, habitats);
        chunk.wilds.reserve(MAX_PER_CHUNK); // sin realojar: Scene guarda punteros a estos pokémon
        for (int i = 0; i < count; ++i) {
            CaptureTarget wild;
            const DirectX::XMFLOAT2 at = pointIn(col, row);
            wild.place(at.x, at.y, RandomUtil::range(-3.14159265f, 3.14159265f));
            assign(wild);
            chunk.wilds.push_back(std::move(wild));
        }
    }

    std::vector<Chunk> m_chunks;
    std::vector<CaptureTarget*> m_loaded;
};
