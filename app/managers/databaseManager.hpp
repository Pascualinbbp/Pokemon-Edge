#pragma once
#include "../utils/databaseUtil.hpp"
#include "../utils/loggerUtil.hpp"
#include "../daos/typeDao.hpp"

class DatabaseManager {
private:
    inline static TypeDao typeDaoInstance;

public:
    static void init() {
        Logger::logInfo("DB_MANAGER", "Inicializando y verificando base de datos local...");
        DatabaseUtil::extractDatabaseIfNeeded();
    }

    static TypeDao& getTypeDao() {
        return typeDaoInstance;
    }
};