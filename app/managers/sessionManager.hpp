#pragma once
#include <chrono>
#include <future>
#include <optional>
#include "saveManager.hpp"
#include "../engine/core/gameEngine.hpp"

// Partida en curso: une el motor de juego con el guardado en ranuras. Generar o cargar el mundo tarda, así que se hace en otro hilo
// (begin*) mientras la pantalla de carga sigue viva; cuando está listo (loadReady) se entrega al motor (finishLoad).
class SessionManager {
    public:
    static GameEngine& engine() { return s_engine; }
    static int activeSlot() { return s_activeSlot; }

    // Partida nueva: ocupa la ranura desde el primer momento.
    static void beginNew(int slot) {
        s_loadSlot = slot;
        s_loadNew = true;
        s_job = std::async(std::launch::async, [] { return s_engine.buildNew(); });
    }

    static void beginSaved(int slot) {
        s_loadSlot = slot;
        s_loadNew = false;
        std::optional<SaveData> save = SaveManager::load(slot);
        if (!save) { s_loadNew = true; s_job = std::async(std::launch::async, [] { return s_engine.buildNew(); }); return; } // partida dañada: de cero
        s_job = std::async(std::launch::async, [data = std::move(*save)] { return s_engine.buildSaved(data); });
    }

    static bool loading() { return s_job.valid(); }
    static bool loadReady() { return s_job.valid() && s_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }

    static void finishLoad() {
        s_engine.adopt(s_job.get());
        s_activeSlot = s_loadSlot;
        if (s_loadNew) SaveManager::save(s_activeSlot, s_engine.captureSave());
    }

    static bool saveCurrent() {
        return SaveManager::save(s_activeSlot, s_engine.captureSave());
    }

    private:
    inline static GameEngine s_engine;
    inline static int s_activeSlot = -1;
    inline static int s_loadSlot = -1;
    inline static bool s_loadNew = false;
    inline static std::future<Scene> s_job;
};
