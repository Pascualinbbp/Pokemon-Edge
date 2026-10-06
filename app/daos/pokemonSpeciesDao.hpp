#pragma once
#include <vector>
#include "../models/pokemonSpecies.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class PokemonSpeciesDao {
    public:
    // Especies con sus habilidades de trabajo.
    std::vector<PokemonSpecies> findAll() const {
        std::vector<PokemonSpecies> species;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, spawn_weight FROM pokemon_species ORDER BY id;")) {
            species.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::real(row, "spawn_weight"), {} });
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT species_id, skill_id, power FROM species_skill ORDER BY species_id;")) {
            const int speciesId = DaoRow::integer(row, "species_id");
            for (PokemonSpecies& entry : species) {
                if (entry.id == speciesId) {
                    entry.skills.push_back({ DaoRow::integer(row, "skill_id"), DaoRow::real(row, "power") });
                    break;
                }
            }
        }
        if (species.empty()) Logger::logError("SPECIES_DAO", "La tabla pokemon_species no existe o está vacía (falta aplicar testing/sql/skill.sql).");
        return species;
    }
};
