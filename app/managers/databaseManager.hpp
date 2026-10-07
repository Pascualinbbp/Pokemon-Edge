#pragma once
#include "../models/gameData.hpp"
#include "../utils/data/databaseUtil.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "../daos/typeDao.hpp"
#include "../daos/pokeballDao.hpp"
#include "../daos/itemDao.hpp"
#include "../daos/chestDao.hpp"
#include "../daos/materialDao.hpp"
#include "../daos/resourceNodeDao.hpp"
#include "../daos/skillDao.hpp"
#include "../daos/pokemonSpeciesDao.hpp"
#include "../daos/groundItemDao.hpp"
#include "../daos/trainingItemDao.hpp"

class DatabaseManager {
    private:
    inline static TypeDao typeDaoInstance;
    inline static PokeballDao pokeballDaoInstance;
    inline static ItemDao itemDaoInstance;
    inline static ChestDao chestDaoInstance;
    inline static MaterialDao materialDaoInstance;
    inline static ResourceNodeDao resourceNodeDaoInstance;
    inline static SkillDao skillDaoInstance;
    inline static PokemonSpeciesDao speciesDaoInstance;
    inline static GroundItemDao groundItemDaoInstance;
    inline static TrainingItemDao trainingItemDaoInstance;

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

    static MaterialDao& getMaterialDao() {
        return materialDaoInstance;
    }

    static ResourceNodeDao& getResourceNodeDao() {
        return resourceNodeDaoInstance;
    }

    static SkillDao& getSkillDao() {
        return skillDaoInstance;
    }

    static PokemonSpeciesDao& getSpeciesDao() {
        return speciesDaoInstance;
    }

    // Único punto de carga de los datos de juego: el motor recibe esto y nada más.
    static GameData loadGameData() {
        GameData data;
        data.balls = pokeballDaoInstance.findAll();
        data.items = itemDaoInstance.findAll();
        data.categories = itemDaoInstance.findCategories();
        data.materials = materialDaoInstance.findAll();
        data.chests = chestDaoInstance.findAll();
        data.nodes = resourceNodeDaoInstance.findAll();
        data.skills = skillDaoInstance.findAll();
        data.tools = skillDaoInstance.findTools();
        data.species = speciesDaoInstance.findAll();
        data.abilities = speciesDaoInstance.findAbilities();
        data.types = typeDaoInstance.findAll();
        data.groundItems = groundItemDaoInstance.findAll();
        data.trainingItems = trainingItemDaoInstance.findAll();
        Logger::logInfo("DB_MANAGER", "Datos de juego: " + std::to_string(data.items.size()) + " objetos, " +
            std::to_string(data.balls.size()) + " pokéballs, " + std::to_string(data.materials.size()) + " materiales, " +
            std::to_string(data.chests.size()) + " cofres, " + std::to_string(data.nodes.size()) + " nodos de recolección, " + std::to_string(data.skills.size()) + " habilidades, " +
            std::to_string(data.tools.size()) + " herramientas, " + std::to_string(data.species.size()) + " pokémon, " + std::to_string(data.abilities.size()) + " habilidades de combate.");
        return data;
    }
};
