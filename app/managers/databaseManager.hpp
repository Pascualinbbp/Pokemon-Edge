#pragma once
#include "../models/gameData.hpp"
#include "../utils/data/databaseUtil.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "../daos/typeDao.hpp"
#include "../daos/pokeballDao.hpp"
#include "../daos/itemDao.hpp"
#include "../daos/chestDao.hpp"

class DatabaseManager {
    private:
    inline static TypeDao typeDaoInstance;
    inline static PokeballDao pokeballDaoInstance;
    inline static ItemDao itemDaoInstance;
    inline static ChestDao chestDaoInstance;

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

    static ItemDao& getItemDao() {
        return itemDaoInstance;
    }

    static ChestDao& getChestDao() {
        return chestDaoInstance;
    }

    // Único punto de carga de los datos de juego: el motor recibe esto y nada más.
    static GameData loadGameData() {
        GameData data;
        data.balls = pokeballDaoInstance.findAll();
        data.items = itemDaoInstance.findAll();
        data.chests = chestDaoInstance.findAll();
        data.zones = chestDaoInstance.findZones();
        return data;
    }
};
