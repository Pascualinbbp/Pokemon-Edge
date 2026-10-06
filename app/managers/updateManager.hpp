#pragma once
#include <mutex>
#include <string>
#include "../utils/network/updateUtil.hpp"
#include "../utils/core/loggerUtil.hpp"

// Estado de la actualización de la aplicación, compartido entre quien la ejecuta (otro hilo, un proceso externo...)
// y la pantalla de actualización. Quien actualiza solo llama a report(); la interfaz lo muestra sola.
class UpdateManager {
    public:
    enum class Phase { IDLE, CHECKING, DOWNLOADING, INSTALLING, RESTARTING, FAILED };

    struct Snapshot {
        Phase phase = Phase::IDLE;
        float progress = 0.0f; // 0..1; negativo = indeterminado
        std::string message;   // detalle (versión, archivo, error...)
    };

    // Informa del avance. Phase::IDLE da la actualización por terminada y la pantalla se cierra.
    static void report(Phase phase, float progress = -1.0f, const std::string& message = {}) {
        const std::lock_guard<std::mutex> lock(mutex());
        state() = { phase, progress, message };
    }

    static Snapshot snapshot() {
        const std::lock_guard<std::mutex> lock(mutex());
        return state();
    }


    // Comprueba la versión remota y, si es distinta, lanza la actualización informando a la pantalla de actualización.
    static void checkAndHandleUpdate() {
        Logger::logInfo("UPDATE_MANAGER", "Verificando si existe una nueva versión disponible...");
        report(Phase::CHECKING);
        try {
            const std::string localVer = UpdateUtil::getLocalVersionString();
            const std::string remoteVer = UpdateUtil::fetchRemoteVersionString();

            if (remoteVer.empty()) {
                Logger::logError("UPDATE_MANAGER", "Error de red: No se pudo obtener version.json remoto.");
                report(Phase::IDLE);
                return;
            }

            Logger::logInfo("UPDATE_MANAGER", "Versión local: " + localVer + " | Versión remota: " + remoteVer);

            if (localVer == remoteVer) {
                Logger::logInfo("UPDATE_MANAGER", "La aplicación ya está en la última versión.");
                report(Phase::IDLE);
                return;
            }

            Logger::logInfo("UPDATE_MANAGER", "Nueva versión detectada. Iniciando proceso de actualización...");
            report(Phase::DOWNLOADING, -1.0f, remoteVer);
            UpdateUtil::executeUpdateScript();
            report(Phase::RESTARTING, -1.0f, remoteVer);
        } catch (const std::exception& e) {
            Logger::logError("UPDATE_MANAGER", std::string("No se pudo verificar versión: ") + e.what());
            report(Phase::IDLE);
        }
    }
    static bool active() { return snapshot().phase != Phase::IDLE; }

    private:
    static std::mutex& mutex() {
        static std::mutex instance;
        return instance;
    }

    static Snapshot& state() {
        static Snapshot instance;
        return instance;
    }
};
