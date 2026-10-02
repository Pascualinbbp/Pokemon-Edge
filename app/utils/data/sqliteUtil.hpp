#pragma once
#include <sqlite3.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/pathsUtil.hpp"
#include "../core/loggerUtil.hpp"

class SqliteUtil {
    public:
    using Row = std::unordered_map<std::string, std::string>;
    using Rows = std::vector<Row>;
    
    static Rows executeSelect(const std::string& sql, const std::vector<std::string>& params = {}) {
        Rows results;
        sqlite3* db = connection();
        if (!db) return results;
        
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) {
            Logger::logError("SQLITE_UTIL", "Error al preparar consulta: " + sql + " (" + sqlite3_errmsg(db) + ")");
            return results;
        }
        std::unique_ptr<sqlite3_stmt, StmtFinalizer> stmt(raw);
        
        for (size_t i = 0; i < params.size(); ++i) {
            sqlite3_bind_text(stmt.get(), static_cast<int>(i + 1), params[i].c_str(),
            static_cast<int>(params[i].size()), SQLITE_TRANSIENT);
        }
        
        // Los nombres de columna se leen una vez, no en cada fila.
        const int cols = sqlite3_column_count(stmt.get());
        std::vector<std::string> names;
        names.reserve(cols);
        for (int i = 0; i < cols; ++i) names.emplace_back(sqlite3_column_name(stmt.get(), i));
        
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            Row row;
            row.reserve(cols);
            for (int i = 0; i < cols; ++i) {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), i));
                row.emplace(names[i], text ? text : "");
            }
            results.push_back(std::move(row));
        }
        return results;
    }
    
    private:
    struct DbCloser { void operator()(sqlite3* db) const { sqlite3_close(db); } };
    struct StmtFinalizer { void operator()(sqlite3_stmt* s) const { sqlite3_finalize(s); } };
    
    // Una única conexión reutilizada (antes se abría y cerraba la BD en cada consulta).
    static sqlite3* connection() {
        static std::unique_ptr<sqlite3, DbCloser> db;
        if (!db) {
            sqlite3* raw = nullptr;
            // u8string(): SQLite espera UTF-8 (C++17).
            if (sqlite3_open_v2(PathsUtil::DB_PATH.u8string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
                Logger::logError("SQLITE_UTIL", std::string("Error al abrir BD: ") + sqlite3_errmsg(raw));
                sqlite3_close(raw);
                return nullptr;
            }
            db.reset(raw);
        }
        return db.get();
    }
};