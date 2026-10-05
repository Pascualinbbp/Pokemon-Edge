#pragma once
#include <cstdlib>
#include <string>
#include <vector>
#include "../models/pokeballType.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "../utils/data/sqliteUtil.hpp"

class PokeballDao {
    public:
    std::vector<PokeballType> findAll() const {
        const auto rows = SqliteUtil::executeSelect(SELECT_ALL);
        std::vector<PokeballType> balls;
        balls.reserve(rows.size());
        for (const auto& row : rows) balls.push_back(fromRow(row));
        if (balls.empty()) Logger::logError("POKEBALL_DAO", "La tabla pokeball no existe o está vacía (falta aplicar testing/sql/pokeball.sql).");
        return balls;
    }

    private:
    static constexpr const char* SELECT_ALL = "SELECT id, name, description, capture_multiplier FROM pokeball ORDER BY id;";

    static const std::string& text(const SqliteUtil::Row& row, const char* key) {
        static const std::string empty;
        const auto it = row.find(key);
        return it == row.end() ? empty : it->second;
    }

    static PokeballType fromRow(const SqliteUtil::Row& row) {
        return { std::atoi(text(row, "id").c_str()), text(row, "name"), text(row, "description"),
                 std::strtof(text(row, "capture_multiplier").c_str(), nullptr) };
    }
};
