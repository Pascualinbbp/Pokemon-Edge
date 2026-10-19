#pragma once
#include <vector>
#include "../models/pokemon/rarity.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class RarityDao {
    public:
    std::vector<Rarity> findAll() const {
        std::vector<Rarity> rarities;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, spawn_multiplier, xp_multiplier FROM rarity ORDER BY id;")) {
            rarities.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::real(row, "spawn_multiplier"), DaoRow::real(row, "xp_multiplier") });
        }
        if (rarities.empty()) Logger::logError("RARITY_DAO", "La tabla rarity no existe o está vacía (falta aplicar testing/sql/rarity.sql).");
        return rarities;
    }
};
