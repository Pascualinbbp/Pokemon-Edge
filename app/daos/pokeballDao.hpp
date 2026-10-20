#pragma once
#include <vector>
#include "../models/items/pokeballType.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class PokeballDao {
    public:
    std::vector<PokeballType> findAll() const {
        const auto rows = SqliteUtil::executeSelect(SELECT_ALL);
        std::vector<PokeballType> balls;
        balls.reserve(rows.size());
        for (const auto& row : rows) {
            balls.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description"),
                              DaoRow::real(row, "capture_multiplier"), DaoRow::real(row, "shiny_multiplier"), DaoRow::integer(row, "obtainable") != 0,
                              DaoRow::text(row, "gift_id").empty() ? -1 : DaoRow::integer(row, "gift_id"), DaoRow::integer(row, "gift_every") });
        }
        if (balls.empty()) Logger::logError("POKEBALL_DAO", "La tabla pokeball no existe o está vacía (falta aplicar testing/sql/pokeball.sql).");
        return balls;
    }

    private:
    static constexpr const char* SELECT_ALL = "SELECT b.id, b.name, b.description, b.capture_multiplier, b.shiny_multiplier, b.obtainable, g.gift_id, COALESCE(g.every, 0) AS gift_every "
        "FROM pokeball b LEFT JOIN pokeball_gift g ON g.bought_id = b.id ORDER BY b.id;";
};
