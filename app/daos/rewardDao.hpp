#pragma once
#include <vector>
#include "../models/world/chestType.hpp"
#include "../models/world/reward.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

// Recompensas (tabla reward) y los cofres que las usan (tabla chest).
class RewardDao {
    public:
    std::vector<RewardEntry> findAll() const {
        std::vector<RewardEntry> rewards;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, kind, target_id, probability, min_quantity, max_quantity FROM reward ORDER BY id, kind, target_id;")) {
            rewards.push_back({ DaoRow::integer(row, "id"), RewardEntry::parse(DaoRow::text(row, "kind")), DaoRow::integer(row, "target_id"),
                                DaoRow::real(row, "probability"), DaoRow::integer(row, "min_quantity"), DaoRow::integer(row, "max_quantity") });
        }
        if (rewards.empty()) Logger::logError("REWARD_DAO", "La tabla reward no existe o está vacía.");
        return rewards;
    }

    std::vector<ChestType> findChests() const {
        std::vector<ChestType> chests;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, rarity_id, reward_id FROM chest ORDER BY id;")) {
            chests.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::integer(row, "rarity_id"), DaoRow::integer(row, "reward_id") });
        }
        if (chests.empty()) Logger::logError("REWARD_DAO", "La tabla chest no existe o está vacía.");
        return chests;
    }
};
