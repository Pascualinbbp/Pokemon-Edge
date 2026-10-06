#pragma once
#include <vector>
#include "../models/ability.hpp"
#include "../models/pokemonSpecies.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class PokemonSpeciesDao {
    public:
    std::vector<PokemonSpecies> findAll() const {
        std::vector<PokemonSpecies> species;
        for (const auto& row : SqliteUtil::executeSelect(SELECT_POKEMON)) {
            PokemonSpecies entry;
            entry.id = DaoRow::integer(row, "id");
            entry.name = DaoRow::text(row, "name");
            entry.description = DaoRow::text(row, "description");
            entry.skillId = DaoRow::integer(row, "skill_id");
            entry.skillLevel = DaoRow::integer(row, "skill_level");
            entry.abilityId = DaoRow::integer(row, "ability_id");
            entry.passiveId = DaoRow::integer(row, "passive_id") > 0 ? DaoRow::integer(row, "passive_id") : -1; // NULL = sin pasiva
            entry.types.push_back(DaoRow::integer(row, "type1_id"));
            if (DaoRow::integer(row, "type2_id") > 0) entry.types.push_back(DaoRow::integer(row, "type2_id"));
            entry.stats = { DaoRow::integer(row, "hp"), DaoRow::integer(row, "attack"), DaoRow::integer(row, "sp_attack"),
                            DaoRow::integer(row, "defense"), DaoRow::integer(row, "sp_defense"), DaoRow::integer(row, "speed") };
            entry.spawnWeight = DaoRow::real(row, "spawn_weight");
            species.push_back(std::move(entry));
        }
        if (species.empty()) Logger::logError("POKEMON_DAO", "La tabla pokemon no existe o está vacía (falta aplicar testing/sql/pokemon.sql).");
        return species;
    }

    std::vector<Ability> findAbilities() const {
        std::vector<Ability> abilities;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, description, floats FROM ability ORDER BY id;")) {
            abilities.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description"),
                                  DaoRow::integer(row, "floats") != 0 });
        }
        return abilities;
    }

    private:
    static constexpr const char* SELECT_POKEMON =
        "SELECT id, name, description, skill_id, skill_level, ability_id, passive_id, type1_id, type2_id, "
        "hp, attack, sp_attack, defense, sp_defense, speed, spawn_weight FROM pokemon ORDER BY id;";
};
