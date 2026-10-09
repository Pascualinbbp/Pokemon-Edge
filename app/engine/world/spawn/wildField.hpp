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
    static constexpr int MAX_PER_CHUNK = 12;        // capacidad fija de cada chunk (las direcciones de sus pokémon no cambian)
    static constexpr float SPAWN_RADIUS = 76.0f;    // zona de aparición, centrada en el jugador (invisible)
    static constexpr float EXCLUSION_RADIUS = 14.0f; // zona pegada al jugador donde nunca nace nada
    static constexpr int MAX_IN_ZONE = 90;          // máximo de pokémon vivos dentro de la zona de aparición
    static constexpr float LOAD_RADIUS = 52.0f;     // dentro de este radio un pokémon aparece
    static constexpr float UNLOAD_RADIUS = 60.0f;   // fuera de este, desaparece (el margen evita parpadeos)
    static constexpr float EDGE = 1.5f;             // separación de los pokémon al borde de su chunk
    static constexpr int POINT_TRIES = 6;

    // Elige especie, nivel y brillo de un pokémon recién colocado.
    template <typename Assign>
    void update(float dt, float playerX, float playerZ, const GameData& data, const HabitatMap& habitats, Assign assign) {
        if (m_chunks.empty()) m_chunks.resize(CHUNKS * CHUNKS);
        m_loaded.clear();

        // Vivos dentro de la zona de aparición y chunks por poblar (los más cercanos primero).
        int alive = 0;
        m_pending.clear();
        for (int row = 0; row < CHUNKS; ++row) {
            for (int col = 0; col < CHUNKS; ++col) {
                Chunk& chunk = m_chunks[row * CHUNKS + col];
                const float distance = distanceTo(col, row, playerX, playerZ);
                chunk.distance = distance;
                if (distance > SPAWN_RADIUS) continue;
                if (!chunk.populated) m_pending.push_back({ distance, row * CHUNKS + col });
                for (const CaptureTarget& wild : chunk.wilds) if (!wild.gone() && !wild.leaving()) ++alive;
            }
        }
        std::sort(m_pending.begin(), m_pending.end());
        for (const auto& entry : m_pending) {
            const int count = countFor(entry.second % CHUNKS, entry.second / CHUNKS, data, habitats);
            if (alive + count > MAX_IN_ZONE) break;
            alive += populate(m_chunks[entry.second], entry.second % CHUNKS, entry.second / CHUNKS, count, playerX, playerZ, assign);
        }

        for (int row = 0; row < CHUNKS; ++row) {
            for (int col = 0; col < CHUNKS; ++col) {
                Chunk& chunk = m_chunks[row * CHUNKS + col];
                if (chunk.dormant && chunk.distance > UNLOAD_RADIUS) continue;

                bool awake = false;
                for (CaptureTarget& wild : chunk.wilds) {
                    if (wild.gone()) {
                        if (wild.refillDue(dt) && chunk.distance <= SPAWN_RADIUS && alive < MAX_IN_ZONE) {
                            DirectX::XMFLOAT2 at;
                            if (pointIn(col, row, playerX, playerZ, at)) {
                                wild.place(at.x, at.y, RandomUtil::range(-3.14159265f, 3.14159265f));
                                assign(wild);
                                ++alive;
                            }
                        }
                        awake = true;
                        continue;
                    }
                    const float dx = wild.body.position.x - playerX, dz = wild.body.position.z - playerZ;
                    const float radius = wild.shown() ? UNLOAD_RADIUS : LOAD_RADIUS;
                    const bool inside = dx * dx + dz * dz <= radius * radius;
                    if (wild.shown() && wild.lifetimeOver(dt)) { wild.leave(playerX, playerZ); --alive; }
                    const bool want = wild.speciesIndex() >= 0 && !wild.leaving() && (wild.capturing() || inside);
                    wild.fade(dt, want);
                    if (wild.shown()) {
                        m_loaded.push_back(&wild);
                        awake = true;
                    } else if (wild.leaving()) {
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
        float distance = 0.0f;
        bool populated = false;
        bool dormant = false; // sin nada cargado ni pendiente de renovar: se salta mientras el jugador esté lejos
    };

    static float distanceTo(int col, int row, float x, float z) {
        const float x0 = -Physics::World::HALF_SIZE + col * CHUNK, z0 = -Physics::World::HALF_SIZE + row * CHUNK;
        const float dx = x - (std::clamp)(x, x0, x0 + CHUNK), dz = z - (std::clamp)(z, z0, z0 + CHUNK);
        return std::sqrt(dx * dx + dz * dz);
    }

    // Punto del chunk fuera de la zona de exclusión del jugador. false si no se encuentra.
    static bool pointIn(int col, int row, float px, float pz, DirectX::XMFLOAT2& out) {
        for (int i = 0; i < POINT_TRIES; ++i) {
            out = { -Physics::World::HALF_SIZE + col * CHUNK + RandomUtil::range(EDGE, CHUNK - EDGE),
                    -Physics::World::HALF_SIZE + row * CHUNK + RandomUtil::range(EDGE, CHUNK - EDGE) };
            const float dx = out.x - px, dz = out.y - pz;
            if (dx * dx + dz * dz > EXCLUSION_RADIUS * EXCLUSION_RADIUS) return true;
        }
        return false;
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
    static int populate(Chunk& chunk, int col, int row, int count, float px, float pz, Assign& assign) {
        chunk.populated = true;
        chunk.wilds.reserve(MAX_PER_CHUNK); // sin realojar: Scene guarda punteros a estos pokémon
        int placed = 0;
        for (int i = 0; i < count; ++i) {
            DirectX::XMFLOAT2 at;
            if (!pointIn(col, row, px, pz, at)) continue;
            CaptureTarget wild;
            wild.place(at.x, at.y, RandomUtil::range(-3.14159265f, 3.14159265f));
            assign(wild);
            chunk.wilds.push_back(std::move(wild));
            ++placed;
        }
        return placed;
    }

    std::vector<Chunk> m_chunks;
    std::vector<CaptureTarget*> m_loaded;
    std::vector<std::pair<float, int>> m_pending;
};
