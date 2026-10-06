#pragma once
#include <vector>
#include "../models/ability.hpp"
#include "../models/element.hpp"
#include "../models/pokemonSpecies.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class PokemonSpeciesDao {
    public:
    // Pokémon con sus tipos, habilidades de combate y habilidades de recolección.
    std::vector<PokemonSpecies> findAll() const {
        std::vector<PokemonSpecies> species;
        for (const auto& row : SqliteUtil::executeSelect(SELECT_POKEMON)) {
            PokemonSpecies entry;
            entry.id = DaoRow::integer(row, "id");
            entry.name = DaoRow::text(row, "name");
            entry.description = DaoRow::text(row, "description");
            entry.stats = { DaoRow::integer(row, "hp"), DaoRow::integer(row, "attack"), DaoRow::integer(row, "sp_attack"),
                            DaoRow::integer(row, "defense"), DaoRow::integer(row, "sp_defense"), DaoRow::integer(row, "speed") };
            entry.spawnWeight = DaoRow::real(row, "spawn_weight");
            species.push_back(std::move(entry));
        }
        for (const auto& row : SqliteUtil::executeSelect(SELECT_ELEMENTS)) {
            if (PokemonSpecies* entry = find(species, DaoRow::integer(row, "pokemon_id"))) entry->elements.push_back(DaoRow::integer(row, "element_id"));
        }
        for (const auto& row : SqliteUtil::executeSelect(SELECT_ABILITIES)) {
            if (PokemonSpecies* entry = find(species, DaoRow::integer(row, "pokemon_id"))) {
                entry->abilities.push_back({ DaoRow::integer(row, "ability_id"), DaoRow::integer(row, "passive") != 0 });
            }
        }
        for (const auto& row : SqliteUtil::executeSelect(SELECT_SKILLS)) {
            if (PokemonSpecies* entry = find(species, DaoRow::integer(row, "pokemon_id"))) {
                entry->skills.push_back({ DaoRow::integer(row, "skill_id"), DaoRow::integer(row, "level") });
            }
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

    std::vector<Element> findElements() const {
        std::vector<Element> elements;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name FROM element ORDER BY id;")) {
            elements.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name") });
        }
        return elements;
    }

    private:
    static PokemonSpecies* find(std::vector<PokemonSpecies>& list, int id) {
        for (PokemonSpecies& entry : list) if (entry.id == id) return &entry;
        return nullptr;
    }

    static constexpr const char* SELECT_POKEMON =
        "SELECT id, name, description, hp, attack, sp_attack, defense, sp_defense, speed, spawn_weight FROM pokemon ORDER BY id;";
    static constexpr const char* SELECT_ELEMENTS = "SELECT pokemon_id, element_id FROM pokemon_element ORDER BY pokemon_id, slot;";
    static constexpr const char* SELECT_ABILITIES = "SELECT pokemon_id, ability_id, passive FROM pokemon_ability ORDER BY pokemon_id, passive, ability_id;";
    static constexpr const char* SELECT_SKILLS = "SELECT pokemon_id, skill_id, level FROM pokemon_skill ORDER BY pokemon_id;";
};
