#pragma once
#include <cstdlib>
#include <string>
#include <vector>
#include "../models/pokeballType.hpp"
#include "../utils/data/sqliteUtil.hpp"

class PokeballDao {
    public:
    // Si la tabla no existe todavía (base de datos antigua), se usan los valores de serie.
    std::vector<PokeballType> findAll() const {
        const auto rows = SqliteUtil::executeSelect(SELECT_ALL);
        std::vector<PokeballType> balls;
        balls.reserve(rows.size());
        for (const auto& row : rows) balls.push_back(fromRow(row));
        return balls.empty() ? defaults() : balls;
    }

    private:
    static constexpr const char* SELECT_ALL =
        "SELECT id, name, capture_multiplier, color_r, color_g, color_b, starting_stock FROM pokeball ORDER BY id;";

    static float number(const SqliteUtil::Row& row, const char* key) {
        const auto it = row.find(key);
        return it == row.end() ? 0.0f : std::strtof(it->second.c_str(), nullptr);
    }

    static PokeballType fromRow(const SqliteUtil::Row& row) {
        const auto name = row.find("name");
        return { static_cast<int>(number(row, "id")), name == row.end() ? std::string() : name->second,
                 number(row, "capture_multiplier"),
                 number(row, "color_r") / 255.0f, number(row, "color_g") / 255.0f, number(row, "color_b") / 255.0f,
                 static_cast<int>(number(row, "starting_stock")) };
    }

    static std::vector<PokeballType> defaults() {
        return {
            { 1, "Poké Ball",  1.0f, 220.0f / 255.0f, 40.0f / 255.0f,  40.0f / 255.0f, 99 },
            { 2, "Super Ball", 1.5f, 40.0f / 255.0f,  100.0f / 255.0f, 230.0f / 255.0f, 50 },
            { 3, "Ultra Ball", 2.0f, 245.0f / 255.0f, 205.0f / 255.0f, 40.0f / 255.0f, 25 },
        };
    }
};
