#pragma once
#include <mutex>
#include <string>

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
