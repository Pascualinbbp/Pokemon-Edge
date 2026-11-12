#pragma once
#include <vector>
#include "../models/pokemon/nature.hpp"
#include "daoRow.hpp"

class NatureDao {
    public:
    std::vector<Nature> findAll() const {
        std::vector<Nature> natures;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, increased_stat, decreased_stat FROM nature ORDER BY id;")) {
            Nature nature;
            nature.id = DaoRow::integer(row, "id");
            nature.name = DaoRow::text(row, "name");
            nature.increased = DaoRow::text(row, "increased_stat").empty() ? -1 : DaoRow::integer(row, "increased_stat");
            nature.decreased = DaoRow::text(row, "decreased_stat").empty() ? -1 : DaoRow::integer(row, "decreased_stat");
            natures.push_back(nature);
        }
        return natures;
    }
};
