#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include "pathsUtil.hpp"
#include "loggerUtil.hpp"

class SqliteUtil {
private:
    static sqlite3* openConnection() {
        sqlite3* db = nullptr;
        if (sqlite3_open(PathsUtil::DB_PATH.string().c_str(), &db) != SQLITE_OK) {
            Logger::logError("SQLITE_UTIL", std::string("Error al abrir BD: ") + sqlite3_errmsg(db));
            if (db) sqlite3_close(db);
            return nullptr;
        }
        return db;
    }

public:
    static std::vector<std::map<std::string, std::string>> executeSelect(const std::string& sql, const std::vector<std::string>& params = {}) {
        std::vector<std::map<std::string, std::string>> results;
        sqlite3* db = openConnection();
        if (!db) return results;

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            
            for (size_t i = 0; i < params.size(); ++i) {
                sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::map<std::string, std::string> row;
                int cols = sqlite3_column_count(stmt);
                for (int i = 0; i < cols; i++) {
                    const char* colName = sqlite3_column_name(stmt, i);
                    const char* colVal = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                    row[colName] = colVal ? colVal : "";
                }
                results.push_back(row);
            }
            sqlite3_finalize(stmt);
        } else {
            Logger::logError("SQLITE_UTIL", "Error al preparar consulta: " + sql);
        }

        sqlite3_close(db);
        return results;
    }
};