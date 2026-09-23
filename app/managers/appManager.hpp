#pragma once
#include "updateManager.hpp"
#include "databaseManager.hpp"
#include "guiManager.hpp"
#include "../utils/loggerUtil.hpp"
#include <exception>

class AppManager {
public:
    AppManager() = default;

    void start() {
        try {
            Logger::logInfo("APP_MANAGER", "Inicializando verificación de sistema...");
            Logger::logInfo("APP_MANAGER", "Llamando a UpdateManager::checkAndHandleUpdate()...");
            UpdateManager::checkAndHandleUpdate();
            Logger::logInfo("APP_MANAGER", "UpdateManager ejecutado sin excepciones.");

            Logger::logInfo("APP_MANAGER", "Llamando a DatabaseManager::init()...");
            DatabaseManager::init();
            Logger::logInfo("APP_MANAGER", "DatabaseManager ejecutado sin excepciones.");

            Logger::logInfo("APP_MANAGER", "Iniciando motor principal...");
            runMainLogic();
        } catch (const std::exception& e) {
            Logger::logError("APP_MANAGER", std::string("Excepción atrapada en AppManager::start(): ") + e.what());
        } catch (...) {
            Logger::logError("APP_MANAGER", "Excepción desconocida atrapada en AppManager::start().");
        }
    }

private:
    void runMainLogic() {
        Logger::logInfo("MAIN_CORE", "¡Bienvenido a Pokemon Edge! Ejecutando GUI...");
        
        Logger::logInfo("MAIN_CORE", "Llamando a GuiManager::init()...");
        GuiManager::init();
        
        Logger::logInfo("MAIN_CORE", "Llamando a GuiManager::run()...");
        GuiManager::run();
        
        Logger::logInfo("MAIN_CORE", "Flujo de GuiManager finalizado.");
    }
};