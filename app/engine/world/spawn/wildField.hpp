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
    static constexpr float SPAWN_RADIUS = 100.0f;   // zona de aparición, centrada en el jugador (invisible)
    static constexpr float EXCLUSION_RADIUS = 30.0f; // zona pegada al jugador donde nunca nace nada
    static constexpr float SPACING = 10.0f;         // distancia mínima entre dos pokémon al nacer (menor que CHUNK)
    static constexpr int MAX_IN_ZONE = 70;          // máximo de pokémon vivos dentro de la zona de aparición
    static constexpr float LOAD_RADIUS = 85.0f;     // dentro de este radio un pokémon aparece
    static constexpr float UNLOAD_RADIUS = 95.0f;   // fuera de este, desaparece (el margen evita parpadeos)
    static constexpr float EDGE = 1.5f;             // separación de los pokémon al borde de su chunk
    static constexpr int POINT_TRIES = 10;

    // Los pokémon solo nacen en el semicírculo que el jugador tiene delante (hacia donde mira la cámara, yaw).
    // assign elige especie, nivel y brillo del recién colocado; devuelve false si ahí no puede vivir ninguno.
    template <typename Assign>
    void update(float dt, float playerX, float playerZ, float yaw, const GameData& data, const HabitatMap& habitats, Assign assign) {
        if (m_chunks.empty()) m_chunks.resize(CHUNKS * CHUNKS);
        m_loaded.clear();
        m_view = { std::sin(yaw), std::cos(yaw) };

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
            alive += populate(entry.second % CHUNKS, entry.second / CHUNKS, count, playerX, playerZ, assign);
            if (alive >= MAX_IN_ZONE) break;
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
                            if (pointIn(col, row, playerX, playerZ, at, &wild)) {
                                wild.place(at.x, at.y, RandomUtil::range(-3.14159265f, 3.14159265f));
                                if (assign(wild)) ++alive;
                                else wild.discard();
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

    // ¿Hay otro pokémon (vivo y no 'ignore') a menos de SPACING de ese punto? Basta mirar los chunks vecinos.
    bool crowded(int col, int row, const DirectX::XMFLOAT2& at, const CaptureTarget* ignore) const {
        for (int r = (std::max)(row - 1, 0); r <= (std::min)(row + 1, CHUNKS - 1); ++r) {
            for (int c = (std::max)(col - 1, 0); c <= (std::min)(col + 1, CHUNKS - 1); ++c) {
                for (const CaptureTarget& other : m_chunks[r * CHUNKS + c].wilds) {
                    if (&other == ignore || other.gone()) continue;
                    const float dx = other.body.position.x - at.x, dz = other.body.position.z - at.y;
                    if (dx * dx + dz * dz < SPACING * SPACING) return true;
                }
            }
        }
        return false;
    }

    // Punto del chunk fuera de la zona de exclusión del jugador y de otros pokémon. false si no se encuentra.
    bool pointIn(int col, int row, float px, float pz, DirectX::XMFLOAT2& out, const CaptureTarget* ignore = nullptr) const {
        for (int i = 0; i < POINT_TRIES; ++i) {
            out = { -Physics::World::HALF_SIZE + col * CHUNK + RandomUtil::range(EDGE, CHUNK - EDGE),
                    -Physics::World::HALF_SIZE + row * CHUNK + RandomUtil::range(EDGE, CHUNK - EDGE) };
            const float dx = out.x - px, dz = out.y - pz;
            const float d2 = dx * dx + dz * dz;
            const bool inFront = dx * m_view.x + dz * m_view.y >= 0.0f; // semicírculo delantero
            if (inFront && d2 > EXCLUSION_RADIUS * EXCLUSION_RADIUS && d2 <= SPAWN_RADIUS * SPAWN_RADIUS && !crowded(col, row, out, ignore)) return true;
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
    int populate(int col, int row, int count, float px, float pz, Assign& assign) {
        Chunk& chunk = m_chunks[row * CHUNKS + col];
        chunk.wilds.reserve(MAX_PER_CHUNK); // sin realojar: Scene guarda punteros a estos pokémon
        int placed = 0;
        for (int i = 0; i < count; ++i) {
            DirectX::XMFLOAT2 at;
            if (!pointIn(col, row, px, pz, at)) continue;
            CaptureTarget wild;
            wild.place(at.x, at.y, RandomUtil::range(-3.14159265f, 3.14159265f));
            if (!assign(wild)) continue;
            chunk.wilds.push_back(std::move(wild));
            ++placed;
        }
        // Un chunk que está detrás del jugador no se da por poblado: se reintenta cuando quede delante.
        chunk.populated = count == 0 || placed > 0;
        return placed;
    }

    DirectX::XMFLOAT2 m_view = { 0.0f, 1.0f }; // hacia dónde mira el jugador (x, z)
    std::vector<Chunk> m_chunks;
    std::vector<CaptureTarget*> m_loaded;
    std::vector<std::pair<float, int>> m_pending;
};
