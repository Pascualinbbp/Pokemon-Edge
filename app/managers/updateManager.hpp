#pragma once
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include "../utils/network/updateUtil.hpp"
#include "../utils/core/loggerUtil.hpp"

// Estado de la actualización de la aplicación, compartido entre el hilo que la ejecuta y la pantalla de actualización.
// Quien actualiza solo llama a report(); la interfaz lo muestra sola.
class UpdateManager {
    public:
    enum class Phase { IDLE, CHECKING, DOWNLOADING, EXTRACTING, INSTALLING, RESTARTING, FAILED };

    struct Snapshot {
        Phase phase = Phase::IDLE;
        float progress = 0.0f; // avance de la fase actual 0..1; negativo = indeterminado
        float overall = 0.0f;  // avance total 0..1
        std::string message;   // detalle (versión, MB, archivos, error...)
    };

    // Informa del avance. Phase::IDLE da la actualización por terminada y la pantalla se cierra.
    static void report(Phase phase, float progress = -1.0f, const std::string& message = {}) {
        const std::lock_guard<std::mutex> lock(mutex());
        state() = { phase, progress, overall(phase, progress), message };
    }

    static Snapshot snapshot() {
        const std::lock_guard<std::mutex> lock(mutex());
        return state();
    }

    // Lanza la comprobación/actualización en un hilo aparte para que la interfaz muestre el avance en directo.
    static void startInBackground() {
        std::thread(&UpdateManager::run).detach();
    }

    // La pantalla de actualización solo aparece cuando hay algo que descargar o instalar.
    static bool active() {
        const Phase phase = snapshot().phase;
        return phase != Phase::IDLE && phase != Phase::CHECKING;
    }

    private:
    // Peso de cada fase en la barra total.
    static float overall(Phase phase, float progress) {
        const float p = std::clamp(progress, 0.0f, 1.0f);
        switch (phase) {
            case Phase::DOWNLOADING: return 0.70f * p;
            case Phase::EXTRACTING:  return 0.70f + 0.25f * p;
            case Phase::INSTALLING:  return 0.95f + 0.05f * p;
            case Phase::RESTARTING:  return 1.0f;
            default:                 return 0.0f;
        }
    }

    static std::string megabytes(uint64_t bytes) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        return buffer;
    }

    static void fail(const std::string& message) {
        Logger::logError("UPDATE_MANAGER", message);
        report(Phase::FAILED, -1.0f, message);
    }

    static void run() {
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
                report(Phase::IDLE);
                return;
            }

            const std::string version = "v" + localVer + "  ->  v" + remoteVer;
            report(Phase::DOWNLOADING, 0.0f, version);
            const bool downloaded = UpdateUtil::downloadPackage([&](uint64_t done, uint64_t total) {
                const std::string text = total ? megabytes(done) + " / " + megabytes(total) : megabytes(done);
                report(Phase::DOWNLOADING, total ? static_cast<float>(done) / static_cast<float>(total) : -1.0f, text);
            });
            if (!downloaded) return fail("No se pudo descargar la actualización.");

            report(Phase::EXTRACTING, 0.0f, "Preparando archivos...");
            const bool extracted = UpdateUtil::extractPackage([&](int done, int total) {
                report(Phase::EXTRACTING, static_cast<float>(done) / static_cast<float>(total),
                    std::to_string(done) + " / " + std::to_string(total) + " archivos");
            });
            if (!extracted) return fail("No se pudo descomprimir la actualización.");

            report(Phase::INSTALLING, 0.0f, version);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            report(Phase::RESTARTING, 1.0f, version);
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            if (!UpdateUtil::installAndRestart()) fail("No se pudo instalar la actualización.");
        } catch (const std::exception& e) {
            Logger::logError("UPDATE_MANAGER", std::string("No se pudo verificar versión: ") + e.what());
            report(Phase::IDLE);
        }
    }

    static std::mutex& mutex() {
        static std::mutex instance;
        return instance;
    }

    static Snapshot& state() {
        static Snapshot instance;
        return instance;
    }
};
