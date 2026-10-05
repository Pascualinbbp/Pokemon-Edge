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

    inline float real(const SqliteUtil::Row& row, const char* key) {
        return std::strtof(text(row, key).c_str(), nullptr);
    }
}
