#pragma once
#include <vector>
#include "../models/pokemon/ability.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"
#include "effectSpecDao.hpp"

// Habilidades (tabla ability) con los parámetros de su efecto (tabla effect): el del mundo (levitar...) y el de combate (cuándo y qué).
class AbilityDao {
    public:
    std::vector<Ability> findAll() const {
        std::vector<Ability> abilities;
        for (const auto& row : SqliteUtil::executeSelect(std::string("SELECT a.id, a.name, a.description, a.trigger, a.priority, e.type_id, COALESCE(e.multiplier, 1) AS multiplier, COALESCE(e.hp_below, 1) AS hp_below, ") +
                                                          EffectSpecDao::COLUMNS + " FROM ability a JOIN effect e ON e.id = a.effect_id ORDER BY a.id;")) {
            Ability ability;
            ability.id = DaoRow::integer(row, "id");
            ability.name = DaoRow::text(row, "name");
            ability.description = DaoRow::text(row, "description");
            ability.effect = Ability::parse(DaoRow::text(row, "kind"));
            ability.typeId = DaoRow::integer(row, "type_id") > 0 ? DaoRow::integer(row, "type_id") : -1;
            ability.multiplier = DaoRow::real(row, "multiplier");
            ability.hpBelow = DaoRow::real(row, "hp_below");
            ability.trigger = Ability::parseTrigger(DaoRow::text(row, "trigger"));
            ability.priority = DaoRow::integer(row, "priority");
            ability.spec = EffectSpecDao::parse(row);
            abilities.push_back(ability);
        }
        if (abilities.empty()) Logger::logError("ABILITY_DAO", "La tabla ability no existe o está vacía.");
        return abilities;
    }
};
