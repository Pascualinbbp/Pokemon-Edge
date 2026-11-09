#pragma once
#include "../models/gameData.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "../engine/audio/radio.hpp"
#include "../engine/world/style/styleRegistry.hpp"
#include "../daos/abilityDao.hpp"
#include "../daos/moveDao.hpp"
#include "../daos/dexRewardDao.hpp"
#include "../daos/generationDao.hpp"
#include "../daos/habitatDao.hpp"
#include "../daos/itemDao.hpp"
#include "../daos/materialDao.hpp"
#include "../daos/pokemonSpeciesDao.hpp"
#include "../daos/radioDao.hpp"
#include "../daos/rarityDao.hpp"
#include "../daos/resourceNodeDao.hpp"
#include "../daos/rewardDao.hpp"
#include "../daos/skillDao.hpp"
#include "../daos/typeDao.hpp"

class DatabaseManager {
    public:
    static void init() {
        Logger::logInfo("DB_MANAGER", "Base de datos incrustada en el exe (se abre en memoria al primer uso).");
    }

    // Único punto de carga de los datos de juego: el motor recibe esto y nada más.
    static GameData loadGameData() {
        GameData data;
        data.materials = MaterialDao().findAll();
        const SkillDao skills;
        data.skills = skills.findAll();
        data.tools = skills.findTools();
        ItemDao().load(data); // objetos, pokéballs, medicinas, piedras (y materiales y herramientas como objetos del inventario)
        const RewardDao rewards;
        data.rewards = rewards.findAll();
        data.chests = rewards.findChests();
        data.nodes = ResourceNodeDao().findAll();
        data.species = PokemonSpeciesDao().findAll();
        data.abilities = AbilityDao().findAll();
        MoveDao().load(data);
        data.rarities = RarityDao().findAll();
        data.generations = GenerationDao().findAll();
        const TypeDao types;
        data.typeChart = types.findChart();
        data.types = types.findAll();
        data.dexRewards = DexRewardDao().findAll();
        const HabitatDao habitats;
        data.habitats = habitats.findHabitats();
        data.weathers = habitats.findWeathers();
        data.fusions = habitats.findFusions();
        Radio::stations() = RadioDao().findAll();
        for (const PokemonType& type : data.types) StyleRegistry::typeColors()[type.id] = type.rgb;
        for (const Material& material : data.materials) StyleRegistry::materialColors()[material.id] = material.rgb;
        Logger::logInfo("DB_MANAGER", "Datos de juego: " + std::to_string(data.items.size()) + " objetos, " + std::to_string(data.balls.size()) + " pokéballs, " +
            std::to_string(data.materials.size()) + " materiales, " + std::to_string(data.chests.size()) + " cofres, " + std::to_string(data.rewards.size()) + " recompensas, " +
            std::to_string(data.nodes.size()) + " nodos de recolección, " + std::to_string(data.skills.size()) + " habilidades de recolección, " + std::to_string(data.tools.size()) +
            " herramientas, " + std::to_string(data.species.size()) + " pokémon, " + std::to_string(data.abilities.size()) + " habilidades, " + std::to_string(data.moves.size()) + " movimientos, " + std::to_string(data.habitats.size()) +
            " hábitats, " + std::to_string(data.weathers.size()) + " climas, " + std::to_string(Radio::stations().size()) + " emisoras.");
        return data;
    }
};
