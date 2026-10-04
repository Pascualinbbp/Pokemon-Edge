#pragma once
#include "../utils/data/databaseUtil.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "../daos/typeDao.hpp"
#include "../daos/pokeballDao.hpp"

class DatabaseManager {
    private:
    inline static TypeDao typeDaoInstance;
    inline static PokeballDao pokeballDaoInstance;
    
    public:
    static void init() {
        Logger::logInfo("DB_MANAGER", "Inicializando y verificando base de datos local...");
        DatabaseUtil::extractDatabaseIfNeeded();
    }
    
    static TypeDao& getTypeDao() {
        return typeDaoInstance;
    }

    static PokeballDao& getPokeballDao() {
        return pokeballDaoInstance;
    }
};
