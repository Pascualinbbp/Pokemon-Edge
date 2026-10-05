#pragma once
#include <vector>
#include "../models/item.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class ItemDao {
    public:
    std::vector<Item> findAll() const {
        std::vector<Item> items;
        for (const auto& row : SqliteUtil::executeSelect(SELECT_ALL)) {
            items.push_back({ DaoRow::integer(row, "id"), ItemCategoryText::parse(DaoRow::text(row, "category")), DaoRow::integer(row, "ref_id") });
        }
        if (items.empty()) Logger::logError("ITEM_DAO", "Las tablas de objetos no existen o están vacías (falta aplicar testing/sql/item.sql).");
        return items;
    }

    private:
    static constexpr const char* SELECT_ALL =
        "SELECT i.id, c.name AS category, i.ref_id FROM item i JOIN item_category c ON c.id = i.category_id ORDER BY i.id;";
};
