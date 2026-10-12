#pragma once
#include <string>

// Acciones con las que el jugador usa una herramienta o interactúa: la animación del personaje y el objeto que sostiene.
// Cada habilidad del mundo (tabla skill, columna animation) dice cuál es la suya; así una habilidad nueva solo necesita su fila.
enum class PlayerAction { NONE, CHOP, MINE, WATER, PICK, FISH, THROW, OPEN };

namespace PlayerActions {
    inline PlayerAction parse(const std::string& name) {
        if (name == "CHOP") return PlayerAction::CHOP;
        if (name == "MINE") return PlayerAction::MINE;
        if (name == "WATER") return PlayerAction::WATER;
        if (name == "PICK") return PlayerAction::PICK;
        if (name == "FISH") return PlayerAction::FISH;
        return PlayerAction::NONE;
    }

    // Segundos que dura una pasada de la animación (un hachazo, un riego...).
    inline float duration(PlayerAction action) {
        switch (action) {
            case PlayerAction::CHOP:  return 0.55f;
            case PlayerAction::MINE:  return 0.6f;
            case PlayerAction::WATER: return 0.9f;
            case PlayerAction::PICK:  return 0.5f;
            case PlayerAction::FISH:  return 0.8f;
            case PlayerAction::THROW: return 0.4f;
            case PlayerAction::OPEN:  return 0.5f;
            default:                  return 0.0f;
        }
    }
}
