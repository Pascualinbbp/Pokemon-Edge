#pragma once
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Reglas de azar de las recompensas (única definición): cofres, objetos del suelo, regalos... todo es un grupo de la tabla reward.
namespace RewardRules {
    // Una entrada del grupo, al azar según su probabilidad. nullptr si el grupo no existe.
    inline const RewardEntry* pick(const GameData& data, int groupId) {
        const auto range = data.rewardGroup(groupId);
        const RewardEntry* chosen = nullptr;
        float total = 0.0f;
        for (const RewardEntry* e = range.first; e != range.second; ++e) total += e->probability;
        if (total <= 0.0f) return nullptr;
        float roll = RandomUtil::range(0.0f, total);
        for (const RewardEntry* e = range.first; e != range.second; ++e) {
            chosen = e;
            if ((roll -= e->probability) < 0.0f) break;
        }
        return chosen;
    }

    // Unidades que da la entrada (entre su mínimo y su máximo).
    inline int quantity(const RewardEntry& entry) { return RandomUtil::integer(entry.minQuantity, entry.maxQuantity); }
}
