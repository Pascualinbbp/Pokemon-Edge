#pragma once
#include <cstdlib>
#include <string>
#include "../utils/data/sqliteUtil.hpp"

// Lectura de columnas de una fila; todos los DAO la comparten.
namespace DaoRow {
    inline const std::string& text(const SqliteUtil::Row& row, const char* key) {
        static const std::string empty;
        const auto it = row.find(key);
        return it == row.end() ? empty : it->second;
    }

    inline int integer(const SqliteUtil::Row& row, const char* key) {
        return std::atoi(text(row, key).c_str());
    }

    // Color guardado como texto hexadecimal "RRGGBB" (con o sin '#'): 0xRRGGBB, o -1 si está vacío.
    inline int rgb(const SqliteUtil::Row& row, const char* key) {
        const std::string& value = text(row, key);
        const char* digits = value.c_str() + (!value.empty() && value[0] == '#' ? 1 : 0);
        return *digits ? static_cast<int>(std::strtol(digits, nullptr, 16)) : -1;
    }

    inline float real(const SqliteUtil::Row& row, const char* key) {
        return std::strtof(text(row, key).c_str(), nullptr);
    }
}
