#pragma once
#include <vector>
#include "../models/items/trainingItem.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class TrainingItemDao {
    public:
    static TrainingItem::Effect effectOf(const std::string& text) {
        return text == "EV" ? TrainingItem::Effect::EV : text == "HEAL" ? TrainingItem::Effect::HEAL : text == "REVIVE" ? TrainingItem::Effect::REVIVE : TrainingItem::Effect::LEVEL;
    }
    std::vector<TrainingItem> findAll() const {
        std::vector<TrainingItem> items;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, description, effect, amount FROM training_item ORDER BY id;")) {
            items.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description"),
                              effectOf(DaoRow::text(row, "effect")),
                              DaoRow::integer(row, "amount") });
        }
        if (items.empty()) Logger::logError("TRAINING_DAO", "La tabla training_item no existe o está vacía (falta aplicar testing/sql/training.sql).");
        return items;
    }
};
