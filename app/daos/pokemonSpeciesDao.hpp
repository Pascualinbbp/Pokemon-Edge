#pragma once
#include <vector>
#include "../models/pokemon/ability.hpp"
#include "../models/pokemon/pokemonSpecies.hpp"
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
            entry.abilityId = DaoRow::integer(row, "ability_id") > 0 ? DaoRow::integer(row, "ability_id") : -1; // NULL = sin habilidad activa
            entry.generationId = DaoRow::integer(row, "generation_id");
            entry.passiveId = DaoRow::integer(row, "passive_id") > 0 ? DaoRow::integer(row, "passive_id") : -1; // NULL = sin pasiva
            entry.types.push_back(DaoRow::integer(row, "type1_id"));
            if (DaoRow::integer(row, "type2_id") > 0) entry.types.push_back(DaoRow::integer(row, "type2_id"));
            entry.stats = { DaoRow::integer(row, "hp"), DaoRow::integer(row, "attack"), DaoRow::integer(row, "sp_attack"),
                            DaoRow::integer(row, "defense"), DaoRow::integer(row, "sp_defense"), DaoRow::integer(row, "speed") };
            entry.catchRate = DaoRow::integer(row, "catch_rate");
            entry.minLevel = DaoRow::integer(row, "min_level");
            entry.maxLevel = DaoRow::integer(row, "max_level");
            entry.swims = DaoRow::integer(row, "swims") != 0;
            entry.starter = DaoRow::integer(row, "starter") != 0;
            species.push_back(std::move(entry));
        }
        if (species.empty()) Logger::logError("POKEMON_DAO", "La tabla pokemon no existe o está vacía (falta aplicar testing/sql/pokemon.sql).");
        loadEvolutions(species);
        loadSpawnProfiles(species);
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
    static void loadEvolutions(std::vector<PokemonSpecies>& species) {
        for (const auto& row : SqliteUtil::executeSelect("SELECT from_id, to_id, method, level, material_id FROM evolution ORDER BY rowid;")) {
            for (PokemonSpecies& entry : species) {
                if (entry.id != DaoRow::integer(row, "from_id")) continue;
                entry.evolutions.push_back({ DaoRow::integer(row, "to_id"), Evolution::parse(DaoRow::text(row, "method")),
                                             DaoRow::integer(row, "level"), DaoRow::integer(row, "material_id") > 0 ? DaoRow::integer(row, "material_id") : -1 });
                break;
            }
        }
    }

    // Dónde y cuándo aparece cada pokémon (testing/sql/habitat.sql). Su rareza es la suma de sus pesos en los hábitats.
    static void loadSpawnProfiles(std::vector<PokemonSpecies>& species) {
        const auto find = [&](const SqliteUtil::Row& row) -> PokemonSpecies* {
            for (PokemonSpecies& entry : species) if (entry.id == DaoRow::integer(row, "pokemon_id")) return &entry;
            return nullptr;
        };
        for (PokemonSpecies& entry : species) entry.spawnWeight = 0.0f;
        for (const auto& row : SqliteUtil::executeSelect("SELECT pokemon_id, habitat_id, base_weight FROM pokemon_habitat;")) {
            if (PokemonSpecies* entry = find(row)) {
                entry->spawn.habitats.push_back({ DaoRow::integer(row, "habitat_id"), DaoRow::real(row, "base_weight") });
                entry->spawnWeight += DaoRow::real(row, "base_weight");
            }
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT pokemon_id, habitat_a_id, habitat_b_id, base_weight FROM pokemon_ecotone;")) {
            if (PokemonSpecies* entry = find(row)) {
                entry->spawn.ecotones.push_back({ DaoRow::integer(row, "habitat_a_id"), DaoRow::integer(row, "habitat_b_id"), DaoRow::real(row, "base_weight") });
            }
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT pokemon_id, kind, weather_id, multiplier FROM pokemon_condition;")) {
            PokemonSpecies* entry = find(row);
            if (!entry) continue;
            const std::string& kind = DaoRow::text(row, "kind");
            const SpawnCondition::Kind type = kind == "day" ? SpawnCondition::Kind::DAY : kind == "night" ? SpawnCondition::Kind::NIGHT : SpawnCondition::Kind::WEATHER;
            entry->spawn.conditions.push_back({ type, DaoRow::integer(row, "weather_id"), DaoRow::real(row, "multiplier") });
        }
        // Sin tablas de hábitats (base de datos antigua) todos aparecen con la misma frecuencia.
        for (PokemonSpecies& entry : species) if (entry.spawnWeight <= 0.0f) entry.spawnWeight = 1.0f;
    }

    static constexpr const char* SELECT_POKEMON =
        "SELECT id, name, description, skill_id, skill_level, ability_id, passive_id, type1_id, type2_id, "
        "hp, attack, sp_attack, defense, sp_defense, speed, catch_rate, min_level, max_level, generation_id, starter, swims FROM pokemon ORDER BY id;";
};
