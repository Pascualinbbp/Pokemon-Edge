#pragma once
#include <vector>
#include "../models/world/habitat.hpp"
#include "../models/world/weather.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

// Hábitats, climas y sus fusiones (testing/sql/habitat.sql).
class HabitatDao {
    public:
    std::vector<Habitat> findHabitats() const {
        std::vector<Habitat> habitats;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, description, min_zones, max_zones, min_radius, max_radius, min_stretch, max_stretch, min_altitude, max_altitude, roughness, placement, beside_id, min_chests, max_chests, daily_chests, min_wild, max_wild, min_nodes, max_nodes, "
                                                         "min_weather_wait, max_weather_wait FROM habitat ORDER BY id;")) {
            Habitat habitat;
            habitat.id = DaoRow::integer(row, "id");
            habitat.name = DaoRow::text(row, "name");
            habitat.description = DaoRow::text(row, "description");
            habitat.minZones = DaoRow::integer(row, "min_zones");
            habitat.maxZones = DaoRow::integer(row, "max_zones");
            habitat.minRadius = DaoRow::real(row, "min_radius");
            habitat.maxRadius = DaoRow::real(row, "max_radius");
            habitat.minStretch = DaoRow::real(row, "min_stretch");
            habitat.maxStretch = DaoRow::real(row, "max_stretch");
            habitat.minAltitude = DaoRow::real(row, "min_altitude");
            habitat.maxAltitude = DaoRow::real(row, "max_altitude");
            habitat.roughness = DaoRow::real(row, "roughness");
            const std::string& placement = DaoRow::text(row, "placement");
            habitat.placement = placement == "COAST" ? HabitatPlacement::COAST : placement == "BESIDE" ? HabitatPlacement::BESIDE : placement == "SEA" ? HabitatPlacement::SEA : HabitatPlacement::ANY;
            habitat.besideId = DaoRow::integer(row, "beside_id") > 0 ? DaoRow::integer(row, "beside_id") : -1;
            habitat.minChests = DaoRow::integer(row, "min_chests");
            habitat.maxChests = DaoRow::integer(row, "max_chests");
            habitat.dailyChests = DaoRow::integer(row, "daily_chests");
            habitat.minWild = DaoRow::integer(row, "min_wild");
            habitat.maxWild = DaoRow::integer(row, "max_wild");
            habitat.minNodes = DaoRow::integer(row, "min_nodes");
            habitat.maxNodes = DaoRow::integer(row, "max_nodes");
            habitat.minWeatherWait = DaoRow::real(row, "min_weather_wait");
            habitat.maxWeatherWait = DaoRow::real(row, "max_weather_wait");
            habitats.push_back(std::move(habitat));
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT habitat_id, weather_id, probability FROM habitat_weather ORDER BY habitat_id;")) {
            for (Habitat& habitat : habitats) {
                if (habitat.id == DaoRow::integer(row, "habitat_id")) habitat.weather.push_back({ DaoRow::integer(row, "weather_id"), DaoRow::real(row, "probability") });
            }
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT habitat_id, node_id, weight FROM habitat_resource ORDER BY habitat_id;")) {
            for (Habitat& habitat : habitats) {
                if (habitat.id == DaoRow::integer(row, "habitat_id")) habitat.resources.push_back({ DaoRow::integer(row, "node_id"), DaoRow::real(row, "weight") });
            }
        }
        if (habitats.empty()) Logger::logError("HABITAT_DAO", "La tabla habitat no existe o está vacía (falta aplicar testing/sql/habitat.sql).");
        return habitats;
    }

    std::vector<Weather> findWeathers() const {
        std::vector<Weather> weathers;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, description FROM weather ORDER BY id;")) {
            weathers.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description") });
        }
        return weathers;
    }

    std::vector<WeatherFusion> findFusions() const {
        std::vector<WeatherFusion> fusions;
        for (const auto& row : SqliteUtil::executeSelect("SELECT weather_a_id, weather_b_id, result_id FROM weather_fusion;")) {
            fusions.push_back({ DaoRow::integer(row, "weather_a_id"), DaoRow::integer(row, "weather_b_id"), DaoRow::integer(row, "result_id") });
        }
        return fusions;
    }
};
