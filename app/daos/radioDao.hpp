#pragma once
#include <vector>
#include "../engine/audio/radio.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

// Emisoras de radio (tabla radio). Su música es assets/audio/radio/<id>.mp3 y su logo assets/sprites/radio/<id>.png.
class RadioDao {
    public:
    std::vector<Radio::Station> findAll() const {
        std::vector<Radio::Station> stations;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, genre FROM radio ORDER BY id;")) {
            stations.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "genre") });
        }
        if (stations.empty()) Logger::logError("RADIO_DAO", "La tabla radio no existe o está vacía.");
        return stations;
    }
};
