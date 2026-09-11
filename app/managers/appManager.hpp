#pragma once
#include "updateManager.hpp"
#include "databaseManager.hpp"
#include "guiManager.hpp"
#include "../utils/loggerUtil.hpp"

class AppManager {
public:
    AppManager() = default;

    void start() {
        Logger::logInfo("APP_MANAGER", "Inicializando verificación de sistema...");
        UpdateManager::checkAndHandleUpdate();

        DatabaseManager::init();

        Logger::logInfo("APP_MANAGER", "Iniciando motor principal...");
        runMainLogic();
    }

private:
    void runMainLogic() {
        Logger::logInfo("MAIN_CORE", "¡Bienvenido a Pokemon Edge! Ejecutando GUI...");
        
        GuiManager::init();
        GuiManager::run();
    }
};