#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <DirectXMath.h>
#include "../entities/chest.hpp"
#include "../habitat/habitatMap.hpp"
#include "../rules/chestRules.hpp"
#include "../state/researchMachine.hpp"
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Cofres del mundo: no se crean al generar el mundo sino poco a poco, cerca del jugador. Cada zona de un hábitat admite a la vez
// entre min_chests y max_chests (se sortea cada día) y cada hábitat tiene un tope de cofres nuevos por día (daily_chests).
// Cuál es el tipo (rareza) de cada cofre es un sorteo aparte (ChestRules).
class ChestField {
    public:
    static constexpr float SPAWN_RADIUS = 70.0f;     // las zonas más cerca que esto del jugador pueden generar cofres
    static constexpr float EXCLUSION_RADIUS = 14.0f; // nunca aparece uno tan cerca del jugador
    static constexpr float INTERVAL = 6.0f;          // segundos entre intentos de generar un cofre
    static constexpr int PLACE_TRIES = 10;
    static constexpr float MIN_LAND = 0.35f;         // altura mínima del terreno para colocar un cofre

    std::vector<Chest> entities;

    // 'day' cambia cada vez que empieza un día: se rehacen los objetivos por zona y los topes de cada hábitat.
    void update(float dt, int day, float playerX, float playerZ, const GameData& data, const HabitatMap& habitats,
                const Physics::World& world) {
        for (Chest& chest : entities) chest.update(dt);
        entities.erase(std::remove_if(entities.begin(), entities.end(), [](const Chest& c) { return c.finished(); }), entities.end());
        if (data.chests.empty() || habitats.zones().empty()) return;

        if (day != m_day) newDay(day, data, habitats);
        m_timer -= dt;
        if (m_timer > 0.0f) return;
        m_timer = INTERVAL * RandomUtil::range(0.6f, 1.4f);

        // Zonas cercanas con sitio libre y tope diario sin agotar.
        std::vector<int> candidates;
        for (size_t z = 0; z < habitats.zones().size(); ++z) {
            const HabitatMap::Zone& zone = habitats.zones()[z];
            const float dx = zone.x - playerX, dz = zone.z - playerZ;
            const float reach = SPAWN_RADIUS + zone.radius;
            if (dx * dx + dz * dz > reach * reach || m_budget[zone.habitat] <= 0) continue;
            if (alive(static_cast<int>(z)) < m_targets[z]) candidates.push_back(static_cast<int>(z));
        }
        if (candidates.empty()) return;

        const int zoneIndex = candidates[RandomUtil::integer(0, static_cast<int>(candidates.size()) - 1)];
        const HabitatMap::Zone& zone = habitats.zones()[zoneIndex];
        for (int attempt = 0; attempt < PLACE_TRIES; ++attempt) {
            // Punto al azar dentro de la elipse de la zona.
            const float angle = RandomUtil::range(0.0f, 6.2831853f), radial = std::sqrt(RandomUtil::range(0.0f, 1.0f));
            const float u = std::cos(angle) * radial * zone.radius * zone.stretch, v = std::sin(angle) * radial * zone.radius / zone.stretch;
            const float x = zone.x + u * zone.cosA - v * zone.sinA, z = zone.z + u * zone.sinA + v * zone.cosA;
            if (!valid(x, z, playerX, playerZ, world)) continue;

            const int type = ChestRules::pickType(data.chests);
            if (type < 0) return;
            entities.emplace_back(zoneIndex, type, data.chests[type].rarity, DirectX::XMFLOAT3{ x, world.groundHeight(x, z), z }, RandomUtil::range(-3.14159265f, 3.14159265f));
            --m_budget[zone.habitat];
            return;
        }
    }

    private:
    int alive(int zone) const {
        int count = 0;
        for (const Chest& chest : entities) if (chest.spot() == zone) ++count;
        return count;
    }

    bool valid(float x, float z, float playerX, float playerZ, const Physics::World& world) const {
        const float half = Physics::World::HALF_SIZE - 2.0f;
        if (std::fabs(x) > half || std::fabs(z) > half || world.groundHeight(x, z) < MIN_LAND) return false; // nunca en el agua
        if ((x - playerX) * (x - playerX) + (z - playerZ) * (z - playerZ) < EXCLUSION_RADIUS * EXCLUSION_RADIUS) return false;
        const float mx = x - ResearchMachine::POSITION.x, mz = z - ResearchMachine::POSITION.z;
        if (mx * mx + mz * mz < 16.0f) return false;
        for (const Physics::World::Box& wall : world.obstacles) {
            if (std::fabs(x - wall.center.x) < wall.half.x + 2.0f && std::fabs(z - wall.center.z) < wall.half.z + 2.0f) return false;
        }
        for (const Chest& chest : entities) {
            const float dx = x - chest.body.position.x, dz = z - chest.body.position.z;
            if (dx * dx + dz * dz < 16.0f) return false;
        }
        return true;
    }

    void newDay(int day, const GameData& data, const HabitatMap& habitats) {
        m_day = day;
        m_budget.assign(data.habitats.size(), 0);
        for (size_t h = 0; h < data.habitats.size(); ++h) m_budget[h] = data.habitats[h].dailyChests;
        m_targets.assign(habitats.zones().size(), 0);
        for (size_t z = 0; z < m_targets.size(); ++z) {
            const int h = habitats.zones()[z].habitat;
            if (h < static_cast<int>(data.habitats.size())) m_targets[z] = RandomUtil::integer(data.habitats[h].minChests, data.habitats[h].maxChests);
        }
    }

    int m_day = -1;
    float m_timer = 0.0f;
    std::vector<int> m_budget;  // cofres que aún puede generar hoy cada hábitat
    std::vector<int> m_targets; // cofres a la vez que admite hoy cada zona
};
