#pragma once
#include <vector>
#include "../models/pokeballType.hpp"
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
                              DaoRow::real(row, "capture_multiplier") });
        }
        if (balls.empty()) Logger::logError("POKEBALL_DAO", "La tabla pokeball no existe o está vacía (falta aplicar testing/sql/pokeball.sql).");
        return balls;
    }

    private:
    static constexpr const char* SELECT_ALL = "SELECT id, name, description, capture_multiplier FROM pokeball ORDER BY id;";
};
