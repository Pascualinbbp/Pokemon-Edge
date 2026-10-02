#pragma once
#include "updateManager.hpp"
#include "databaseManager.hpp"
#include "guiManager.hpp"
#include "../utils/core/loggerUtil.hpp"

class AppManager {
public:
    void start() {
        UpdateManager::checkAndHandleUpdate();
        DatabaseManager::init();

        Logger::logInfo("APP_MANAGER", "¡Bienvenido a Pokemon Edge! Ejecutando GUI...");
        GuiManager::init();
        GuiManager::run();
        Logger::logInfo("APP_MANAGER", "Flujo de GuiManager finalizado.");
    }
};