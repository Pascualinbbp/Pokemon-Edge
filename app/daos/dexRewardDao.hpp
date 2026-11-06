#pragma once
#include <vector>
#include "../models/pokemon/dexReward.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

// Hitos de la Pokédex (tabla dex_milestone); su premio de experiencia está en su grupo de la tabla reward.
class DexRewardDao {
    public:
    std::vector<DexReward> findAll() const {
        std::vector<DexReward> rewards;
        for (const auto& row : SqliteUtil::executeSelect("SELECT d.id, d.kind, d.threshold, d.description, d.progress, d.species_id, d.reward_id, "
                                                          "COALESCE((SELECT r.min_quantity FROM reward r WHERE r.id = d.reward_id AND r.kind = 'XP'), 0) AS player_xp FROM dex_milestone d ORDER BY d.id;")) {
            rewards.push_back({ DaoRow::integer(row, "id"), DexReward::parse(DaoRow::text(row, "kind")), DaoRow::integer(row, "threshold"),
                                DaoRow::text(row, "description"), DaoRow::integer(row, "player_xp"), DaoRow::integer(row, "progress"),
                                DaoRow::text(row, "species_id").empty() ? -1 : DaoRow::integer(row, "species_id") });
        }
        if (rewards.empty()) Logger::logError("DEX_REWARD_DAO", "La tabla dex_milestone no existe o está vacía.");
        return rewards;
    }
};
