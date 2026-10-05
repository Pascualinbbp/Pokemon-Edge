#pragma once
#include <string>
#include "../../models/chestType.hpp"
#include "../world/inventory.hpp"

// Avisos grandes en pantalla.
enum class Notice { NONE, CAPTURED, ESCAPED, LUCKY, SUPER_LUCKY, OUT_OF_STOCK, REWARD };

// Estado del juego que la interfaz necesita mostrar (el HUD no conoce la escena).
struct GameStatus {
    float aimBlend = 0.0f;      // 0 = cámara normal, 1 = cámara de apuntado completa
    int captures = 0;           // capturas de esta sesión
    Notice notice = Notice::NONE;
    bool locked = false;        // cámara fijada a un objetivo
    const Inventory* inventory = nullptr; // inventario (válido durante el frame)
    const ChestType* nearbyChest = nullptr;      // cofre cerrado al alcance del jugador (para el aviso de interacción)
    const std::string* rewardText = nullptr;     // texto de la última recompensa (con Notice::REWARD)

    // Pokémon al que se apunta dentro del rango de lanzamiento.
    bool hasAimTarget = false;
    int chancePercent = 0;      // porcentaje de captura (ya con bonificaciones)
    bool behind = false;        // bonificación por la espalda
    bool hidden = false;        // bonificación por sigilo (agachado)
};
