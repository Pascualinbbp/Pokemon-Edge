#pragma once
#include <vector>
#include "../models/world/resourceNodeType.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class ResourceNodeDao {
    public:
    std::vector<ResourceNodeType> findAll() const {
        std::vector<ResourceNodeType> nodes;
        for (const auto& row : SqliteUtil::executeSelect(SELECT_ALL)) {
            nodes.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "action"),
                              DaoRow::integer(row, "material_id"), DaoRow::integer(row, "skill_id"), DaoRow::integer(row, "level"), DaoRow::integer(row, "hits"), DaoRow::integer(row, "grow_seconds"),
                              DaoRow::integer(row, "regrow_seconds"), DaoRow::integer(row, "min_yield"), DaoRow::integer(row, "max_yield") });
        }
        if (nodes.empty()) Logger::logError("RESOURCE_NODE_DAO", "La tabla resource_node no existe o está vacía (falta aplicar testing/sql/material.sql).");
        return nodes;
    }

    private:
    static constexpr const char* SELECT_ALL =
        "SELECT id, name, action, material_id, skill_id, level, hits, grow_seconds, regrow_seconds, min_yield, max_yield FROM resource_node ORDER BY id;";
};
