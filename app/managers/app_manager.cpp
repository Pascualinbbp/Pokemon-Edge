#include "app_manager.hpp"
#include "../utils/update_util.hpp"
#include "../utils/logger_util.hpp"

AppManager::AppManager() {}

void AppManager::start() {
    Logger::logInfo("APP_MANAGER", "Inicializando verificación de sistema...");

    if (!UpdateUtil::isLatestVersion()) {
        UpdateUtil::launchUpdateProcess();
    } else {
        Logger::logInfo("APP_MANAGER", "Iniciando motor principal...");
        runMainLogic();
    }
}

void AppManager::runMainLogic() {
    Logger::logInfo("MAIN_CORE", "¡Bienvenido a PokemonEdge! Ejecutando aplicación...");
    // Bucle principal / inicialización GUI
}