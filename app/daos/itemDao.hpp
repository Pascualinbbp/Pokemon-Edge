#pragma once
#include <vector>
#include "../models/items/item.hpp"
#include "../models/items/itemCategoryInfo.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class ItemDao {
    public:
    std::vector<Item> findAll() const {
        std::vector<Item> items;
        for (const auto& row : SqliteUtil::executeSelect(SELECT_ALL)) {
            items.push_back({ DaoRow::integer(row, "id"), ItemCategoryText::parse(DaoRow::text(row, "category")),
                              DaoRow::integer(row, "category_id"), DaoRow::integer(row, "ref_id"),
                              DaoRow::integer(row, "buy_price"), DaoRow::integer(row, "sell_price") });
        }
        if (items.empty()) Logger::logError("ITEM_DAO", "Las tablas de objetos no existen o están vacías (falta aplicar testing/sql/item.sql).");
        return items;
    }

    std::vector<ItemCategoryInfo> findCategories() const {
        std::vector<ItemCategoryInfo> categories;
        for (const auto& row : SqliteUtil::executeSelect(SELECT_CATEGORIES)) {
            categories.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "label") });
        }
        return categories;
    }

    private:
    static constexpr const char* SELECT_ALL =
        "SELECT i.id, c.name AS category, i.category_id, i.ref_id, i.buy_price, i.sell_price FROM item i JOIN item_category c ON c.id = i.category_id ORDER BY i.id;";
    static constexpr const char* SELECT_CATEGORIES = "SELECT id, name, label FROM item_category ORDER BY id;";
};
