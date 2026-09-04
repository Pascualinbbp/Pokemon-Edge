#pragma once
#include <string>
#include "../utils/updateUtil.hpp"
#include "../utils/loggerUtil.hpp"

class UpdateManager {
public:
    static void checkAndHandleUpdate() {
        Logger::logInfo("UPDATE_MANAGER", "Verificando si existe una nueva versión disponible...");
        try {
            std::string localVer = UpdateUtil::getLocalVersionString();
            std::string remoteVer = UpdateUtil::fetchRemoteVersionString();

            if (remoteVer.empty()) {
                Logger::logError("UPDATE_MANAGER", "Error de red: No se pudo obtener version.json remoto.");
                return;
            }

            Logger::logInfo("UPDATE_MANAGER", "Versión local: " + localVer + " | Versión remota: " + remoteVer);

            if (localVer == remoteVer) {
                Logger::logInfo("UPDATE_MANAGER", "La aplicación ya está en la última versión.");
            } else {
                Logger::logInfo("UPDATE_MANAGER", "Nueva versión detectada. Iniciando proceso de actualización...");
                UpdateUtil::executeUpdateScript();
            }
        } catch (const std::exception& e) {
            Logger::logError("UPDATE_MANAGER", std::string("No se pudo verificar versión: ") + e.what());
        }
    }
};