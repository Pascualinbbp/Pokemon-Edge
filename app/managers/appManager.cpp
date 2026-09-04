#include "appManager.hpp"
#include "updateManager.hpp"
#include "../utils/databaseUtil.hpp"
#include "../utils/loggerUtil.hpp"

AppManager::AppManager() {}

void AppManager::start() {
    Logger::logInfo("APP_MANAGER", "Inicializando verificación de sistema...");

    UpdateManager::checkAndHandleUpdate();

    Logger::logInfo("APP_MANAGER", "Extrayendo base de datos local si es necesario...");
    DatabaseUtil::extractDatabaseIfNeeded();

    Logger::logInfo("APP_MANAGER", "Iniciando motor principal...");
    runMainLogic();
}

void AppManager::runMainLogic() {
    Logger::logInfo("MAIN_CORE", "¡Bienvenido a Pokemon Edge! Ejecutando aplicación...");
    // Bucle principal / inicialización GUI
}