#pragma once
#include <vector>
#include "../models/item.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class ItemDao {
    public:
    std::vector<Item> findAll() const {
        const auto rows = SqliteUtil::executeSelect(SELECT_ALL);
        std::vector<Item> items;
        items.reserve(rows.size());
        for (const auto& row : rows) {
            items.push_back({ DaoRow::integer(row, "id"), ItemCategoryText::parse(DaoRow::text(row, "category")),
                              DaoRow::integer(row, "ref_id"), DaoRow::text(row, "name"), DaoRow::text(row, "description") });
        }
        if (items.empty()) Logger::logError("ITEM_DAO", "La vista item_info no existe o está vacía (falta aplicar testing/sql/item.sql).");
        return items;
    }

    private:
    static constexpr const char* SELECT_ALL = "SELECT id, category, ref_id, name, description FROM item_info ORDER BY id;";
};
