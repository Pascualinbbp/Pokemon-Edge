#pragma once
#include <string>
#include <vector>
#include "../utils/sqliteUtil.hpp"

struct PokemonType {
    int id;
    std::string name;
};

class TypeDao {
public:
    PokemonType findById(int id) {
        PokemonType pType = { -1, "" };
        std::string sql = "SELECT id, name FROM type WHERE id = ?;";
        std::vector<std::string> params = { std::to_string(id) };

        auto results = SqliteUtil::executeSelect(sql, params);
        
        if (!results.empty()) {
            pType.id = std::stoi(results[0]["id"]);
            pType.name = results[0]["name"];
        }
        return pType;
    }

    std::vector<PokemonType> findAll() {
        std::vector<PokemonType> types;
        std::string sql = "SELECT id, name FROM type;";

        auto results = SqliteUtil::executeSelect(sql);
        
        for (const auto& row : results) {
            PokemonType pType;
            pType.id = std::stoi(row.at("id"));
            pType.name = row.at("name");
            types.push_back(pType);
        }
        return types;
    }
};