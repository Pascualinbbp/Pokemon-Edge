#pragma once
#include <vector>
#include "../../models/chestType.hpp"
#include "../../utils/core/randomUtil.hpp"

// Reglas de azar de los cofres (única definición).
namespace ChestRules {
    // Qué tipo de cofre aparece (según spawn_weight). -1 si no hay tipos.
    inline int pickType(const std::vector<ChestType>& chests) {
        return RandomUtil::weightedIndex(chests, [](const ChestType& chest) { return chest.spawnWeight; });
    }

    // La recompensa de un cofre: una sola, al azar según su peso. nullptr si no tiene.
    inline const ChestReward* pickReward(const ChestType& chest) {
        const int index = RandomUtil::weightedIndex(chest.rewards, [](const ChestReward& reward) { return reward.weight; });
        return index < 0 ? nullptr : &chest.rewards[index];
    }
}
