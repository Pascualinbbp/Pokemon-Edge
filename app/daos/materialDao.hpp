#pragma once
#include <vector>
#include "../models/material.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class MaterialDao {
    public:
    std::vector<Material> findAll() const {
        std::vector<Material> materials;
        for (const auto& row : SqliteUtil::executeSelect(SELECT_ALL)) {
            materials.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description") });
        }
        if (materials.empty()) Logger::logError("MATERIAL_DAO", "La tabla material no existe o está vacía (falta aplicar testing/sql/material.sql).");
        return materials;
    }

    private:
    static constexpr const char* SELECT_ALL = "SELECT id, name, description FROM material ORDER BY id;";
};
