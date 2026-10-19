#pragma once
#include <string>
#include <vector>
#include "../models/pokemon/pokemonType.hpp"
#include "../models/pokemon/typeChart.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"
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
    
    // Tabla de efectividades entre tipos (lo que no está en la tabla es x1).
    TypeChart findChart() const {
        TypeChart chart;
        for (const auto& row : SqliteUtil::executeSelect("SELECT attacker_id, defender_id, multiplier FROM type_matchup;")) {
            chart.add(DaoRow::integer(row, "attacker_id"), DaoRow::integer(row, "defender_id"), DaoRow::real(row, "multiplier"));
        }
        if (chart.empty()) Logger::logError("TYPE_DAO", "La tabla type_matchup no existe o está vacía (falta aplicar testing/sql/type.sql): todos los ataques harán x1.");
        return chart;
    }

    private:
    static constexpr const char* SELECT_ALL = "SELECT id, name FROM type;";
    static constexpr const char* SELECT_BY_ID = "SELECT id, name FROM type WHERE id = ?;";
    
    static PokemonType fromRow(const SqliteUtil::Row& row) {
        return { std::stoi(row.at("id")), row.at("name") };
    }
};
