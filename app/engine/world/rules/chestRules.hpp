#pragma once
#include <vector>
#include "../../../models/world/chestType.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Reglas de azar de los cofres (única definición).
namespace ChestRules {
    // Qué tipo de cofre aparece (según spawn_weight). -1 si no hay tipos.
    inline int pickType(const std::vector<ChestType>& chests) {
        return RandomUtil::weightedIndex(chests, [](const ChestType& chest) { return chest.spawnWeight; });
    }

    // La recompensa de un cofre: una sola, al azar según su probabilidad. nullptr si no tiene.
    inline const ChestReward* pickReward(const ChestType& chest) {
        return RandomUtil::pick(chest.rewards, [](const ChestReward& reward) { return reward.probability; });
    }
}
