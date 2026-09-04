#include "appManager.hpp"
#include "updateManager.hpp"
#include "databaseManager.hpp"
#include "guiManager.hpp"
#include "../utils/loggerUtil.hpp"
#include <iostream>

AppManager::AppManager() {}

void AppManager::start() {
    Logger::logInfo("APP_MANAGER", "Inicializando verificación de sistema...");
    UpdateManager::checkAndHandleUpdate();

    DatabaseManager::init();

    Logger::logInfo("APP_MANAGER", "Iniciando motor principal...");
    runMainLogic();
}

void AppManager::runMainLogic() {
    Logger::logInfo("MAIN_CORE", "¡Bienvenido a Pokemon Edge! Ejecutando GUI...");
    
    // En lugar de la consola, iniciamos el manager de la interfaz
    GuiManager::init();
    GuiManager::run();
}