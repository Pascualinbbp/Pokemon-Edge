#pragma once
#include <vector>
#include "../models/pokemon/ability.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

// Habilidades (tabla ability) con los parámetros de su efecto (tabla effect).
class AbilityDao {
    public:
    std::vector<Ability> findAll() const {
        std::vector<Ability> abilities;
        for (const auto& row : SqliteUtil::executeSelect("SELECT a.id, a.name, a.description, e.kind, e.type_id, COALESCE(e.multiplier, 1) AS multiplier, COALESCE(e.hp_below, 1) AS hp_below "
                                                          "FROM ability a JOIN effect e ON e.id = a.effect_id ORDER BY a.id;")) {
            abilities.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description"), Ability::parse(DaoRow::text(row, "kind")),
                                  DaoRow::integer(row, "type_id") > 0 ? DaoRow::integer(row, "type_id") : -1, DaoRow::real(row, "multiplier"), DaoRow::real(row, "hp_below") });
        }
        if (abilities.empty()) Logger::logError("ABILITY_DAO", "La tabla ability no existe o está vacía.");
        return abilities;
    }
};
