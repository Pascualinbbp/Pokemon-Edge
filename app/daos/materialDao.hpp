#pragma once
#include <vector>
#include "../models/items/material.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class MaterialDao {
    public:
    std::vector<Material> findAll() const {
        std::vector<Material> materials;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, description, category_id, buy_price, sell_price, color FROM material ORDER BY id;")) {
            materials.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description"), DaoRow::integer(row, "category_id"),
                                  DaoRow::integer(row, "buy_price"), DaoRow::integer(row, "sell_price"), DaoRow::rgb(row, "color") });
        }
        if (materials.empty()) Logger::logError("MATERIAL_DAO", "La tabla material no existe o está vacía.");
        return materials;
    }
};
