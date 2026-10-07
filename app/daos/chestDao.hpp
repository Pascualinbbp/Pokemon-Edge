#pragma once
#include <vector>
#include "../models/chestType.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class ChestDao {
    public:
    // Tipos de cofre con sus recompensas posibles.
    std::vector<ChestType> findAll() const {
        std::vector<ChestType> chests;
        for (const auto& row : SqliteUtil::executeSelect(SELECT_CHESTS)) {
            chests.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::integer(row, "rarity_id"),
                               DaoRow::real(row, "spawn_weight"), {} });
        }
        for (const auto& row : SqliteUtil::executeSelect(SELECT_REWARDS)) {
            const int chestId = DaoRow::integer(row, "chest_id");
            for (ChestType& chest : chests) {
                if (chest.id == chestId) {
                    chest.rewards.push_back({ DaoRow::integer(row, "item_id"), DaoRow::integer(row, "min_quantity"), DaoRow::integer(row, "max_quantity"), DaoRow::real(row, "probability") });
                    break;
                }
            }
        }
        if (chests.empty()) Logger::logError("CHEST_DAO", "Las tablas de cofres no existen o están vacías (falta aplicar testing/sql/chest.sql).");
        return chests;
    }

    private:
    static constexpr const char* SELECT_CHESTS = "SELECT id, name, rarity_id, spawn_weight FROM chest ORDER BY id;";
    static constexpr const char* SELECT_REWARDS = "SELECT chest_id, item_id, min_quantity, max_quantity, probability FROM chest_reward ORDER BY id;";
};
