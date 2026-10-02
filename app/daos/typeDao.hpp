#pragma once
#include <string>
#include <vector>
#include "../models/pokemonType.hpp"
#include "../utils/data/sqliteUtil.hpp"

class TypeDao {
public:
    PokemonType findById(int id) const {
        const auto rows = SqliteUtil::executeSelect(SELECT_BY_ID, { std::to_string(id) });
        return rows.empty() ? PokemonType{} : fromRow(rows.front());
    }

    std::vector<PokemonType> findAll() const {
        const auto rows = SqliteUtil::executeSelect(SELECT_ALL);
        std::vector<PokemonType> types;
        types.reserve(rows.size());
        for (const auto& row : rows) types.push_back(fromRow(row));
        return types;
    }

private:
    static constexpr const char* SELECT_ALL = "SELECT id, name FROM type;";
    static constexpr const char* SELECT_BY_ID = "SELECT id, name FROM type WHERE id = ?;";

    static PokemonType fromRow(const SqliteUtil::Row& row) {
        return { std::stoi(row.at("id")), row.at("name") };
    }
};