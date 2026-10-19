#pragma once
#include <vector>
#include "../models/pokemon/generation.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class GenerationDao {
    public:
    std::vector<Generation> findAll() const {
        std::vector<Generation> generations;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, region FROM generation ORDER BY id;")) {
            generations.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "region") });
        }
        return generations;
    }
};
