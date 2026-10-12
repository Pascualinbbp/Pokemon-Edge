#pragma once
#include <vector>
#include "../models/pokemon/dexReward.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class DexRewardDao {
    public:
    std::vector<DexReward> findAll() const {
        std::vector<DexReward> rewards;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, kind, threshold, description, item_id, quantity, species_id FROM dex_reward ORDER BY id;")) {
            rewards.push_back({ DaoRow::integer(row, "id"), DexReward::parse(DaoRow::text(row, "kind")), DaoRow::integer(row, "threshold"),
                                DaoRow::text(row, "description"), DaoRow::integer(row, "item_id"), DaoRow::integer(row, "quantity"),
                                DaoRow::text(row, "species_id").empty() ? -1 : DaoRow::integer(row, "species_id") });
        }
        if (rewards.empty()) Logger::logError("DEX_REWARD_DAO", "La tabla dex_reward no existe o está vacía (falta aplicar testing/sql/dex_reward.sql).");
        return rewards;
    }
};
