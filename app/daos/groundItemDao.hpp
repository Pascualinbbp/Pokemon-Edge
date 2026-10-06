#pragma once
#include <vector>
#include "../models/groundItemType.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class GroundItemDao {
    public:
    std::vector<GroundItemType> findAll() const {
        std::vector<GroundItemType> items;
        for (const auto& row : SqliteUtil::executeSelect("SELECT item_id, min_quantity, max_quantity, spawn_weight FROM ground_item ORDER BY id;")) {
            items.push_back({ DaoRow::integer(row, "item_id"), DaoRow::integer(row, "min_quantity"), DaoRow::integer(row, "max_quantity"),
                              DaoRow::real(row, "spawn_weight") });
        }
        if (items.empty()) Logger::logError("GROUND_ITEM_DAO", "La tabla ground_item no existe o está vacía (falta aplicar testing/sql/ground_item.sql).");
        return items;
    }
};
