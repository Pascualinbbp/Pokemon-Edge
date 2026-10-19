#pragma once
#include <vector>
#include "../models/pokemon/ability.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class AbilityDao {
    public:
    std::vector<Ability> findAll() const {
        std::vector<Ability> abilities;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, description, effect, type_id, multiplier, hp_below FROM ability ORDER BY id;")) {
            abilities.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description"), Ability::parse(DaoRow::text(row, "effect")),
                                  DaoRow::integer(row, "type_id") > 0 ? DaoRow::integer(row, "type_id") : -1, DaoRow::real(row, "multiplier"), DaoRow::real(row, "hp_below") });
        }
        if (abilities.empty()) Logger::logError("ABILITY_DAO", "La tabla ability no existe o está vacía (falta aplicar testing/sql/ability.sql).");
        return abilities;
    }
};
