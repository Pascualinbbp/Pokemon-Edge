#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <DirectXMath.h>
#include "groundSpawn.hpp"
#include "../entities/groundItem.hpp"
#include "../habitat/habitatMap.hpp"
#include "../state/researchMachine.hpp"
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Objetos sueltos por todo el mundo: aparecen poco a poco cerca del jugador y desaparecen al quedar lejos.
// Qué dan depende del hábitat donde caen: el grupo de la tabla reward indicado en habitat.ground_reward_id
// (si no tiene, el común, RewardGroup::GROUND). Así cada clima ofrece sus propias semillas y objetos.
namespace GroundField {
    inline constexpr float SPAWN_MIN = 8.0f;     // nunca aparecen más cerca del jugador
    inline constexpr float SPAWN_MAX = 55.0f;
    inline constexpr float DESPAWN = 95.0f;      // más lejos que esto se retiran
    inline constexpr float INTERVAL = 2.5f;      // segundos entre intentos
    inline constexpr int MAX_NEARBY = 28;
    inline constexpr int TRIES = 8;
    inline constexpr float MIN_LAND_DEPTH = -50.0f;
    inline constexpr float MAX_SLOPE = 0.6f;
    inline constexpr float SPACING = 3.0f;       // separación mínima entre dos objetos

    // Grupo de recompensas que corresponde a un punto del mundo.
    inline int groupAt(const GameData& data, const HabitatMap& habitats, float x, float z) {
        const int h = habitats.dominant(x, z);
        if (h >= 0 && h < static_cast<int>(data.habitats.size()) && data.habitats[h].groundRewardId > 0) return data.habitats[h].groundRewardId;
        return RewardGroup::GROUND;
    }

    inline bool valid(float x, float z, float playerX, float playerZ, const Physics::World& world, const std::vector<GroundItem>& items) {
        if (world.waterDepth(x, z) > 0.05f || world.groundHeight(x, z) < MIN_LAND_DEPTH || world.slope(x, z) > MAX_SLOPE) return false;
        if (HabitatMap::inCity(x, z)) return false;
        const float mx = x - ResearchMachine::POSITION.x, mz = z - ResearchMachine::POSITION.z;
        if (mx * mx + mz * mz < 16.0f) return false;
        for (const Physics::World::Box& wall : world.obstacles) {
            if (std::fabs(x - wall.center.x) < wall.half.x + 1.0f && std::fabs(z - wall.center.z) < wall.half.z + 1.0f) return false;
        }
        for (const GroundItem& item : items) {
            const float dx = x - item.body.position.x, dz = z - item.body.position.z;
            if (dx * dx + dz * dz < SPACING * SPACING) return false;
        }
        return true;
    }

    // Avanza el reloj de aparición; 'timer' lo guarda quien llama. Añade a 'items' (spot -1: sin punto fijo) y retira los lejanos.
    inline void update(float dt, float& timer, float playerX, float playerZ, bool underground, const GameData& data,
                       const HabitatMap& habitats, const Physics::World& world, std::vector<GroundItem>& items) {
        items.erase(std::remove_if(items.begin(), items.end(), [&](const GroundItem& item) {
            if (item.spot() >= 0) return false;
            const float dx = item.body.position.x - playerX, dz = item.body.position.z - playerZ;
            return dx * dx + dz * dz > DESPAWN * DESPAWN;
        }), items.end());
        if (underground) return;
        timer -= dt;
        if (timer > 0.0f) return;
        timer = INTERVAL * RandomUtil::range(0.6f, 1.4f);

        int nearby = 0;
        for (const GroundItem& item : items) if (item.spot() < 0 && item.available()) ++nearby;
        if (nearby >= MAX_NEARBY) return;

        for (int attempt = 0; attempt < TRIES; ++attempt) {
            const float angle = RandomUtil::range(0.0f, 6.2831853f);
            const float radius = std::sqrt(RandomUtil::range(SPAWN_MIN * SPAWN_MIN, SPAWN_MAX * SPAWN_MAX));
            const float x = playerX + std::cos(angle) * radius, z = playerZ + std::sin(angle) * radius;
            if (!valid(x, z, playerX, playerZ, world, items)) continue;
            const RewardEntry* reward = RewardRules::pick(data, groupAt(data, habitats, x, z));
            if (!reward) continue;
            GroundItem item(-1, DirectX::XMFLOAT3{ x, world.groundHeight(x, z) + 0.3f, z });
            item.setReward(static_cast<int>(reward - data.rewards.data()));
            items.push_back(std::move(item));
            return;
        }
    }
}
