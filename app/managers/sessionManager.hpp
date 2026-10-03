#pragma once
#include <optional>
#include "saveManager.hpp"
#include "../engine/core/gameEngine.hpp"

// Partida en curso: une el motor de juego con el guardado en ranuras.
class SessionManager {
    public:
    static GameEngine& engine() { return s_engine; }
    static int activeSlot() { return s_activeSlot; }

    // Partida nueva: ocupa la ranura desde el primer momento.
    static void startNew(int slot) {
        s_engine.newGame();
        SaveManager::save(slot, s_engine.captureSave());
        s_activeSlot = slot;
    }

    static void startSaved(int slot) {
        if (const std::optional<SaveData> save = SaveManager::load(slot)) s_engine.applySave(*save);
        else s_engine.newGame(); // partida dañada: se empieza de cero en esa ranura
        s_activeSlot = slot;
    }

    static bool saveCurrent() {
        return SaveManager::save(s_activeSlot, s_engine.captureSave());
    }

    private:
    inline static GameEngine s_engine;
    inline static int s_activeSlot = -1;
};