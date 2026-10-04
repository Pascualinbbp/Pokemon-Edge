#pragma once
#include "../world/inventory.hpp"

// Estado del juego que la interfaz necesita mostrar (el HUD no conoce la escena).
struct GameStatus {
    float aimBlend = 0.0f;      // 0 = cámara normal, 1 = cámara de apuntado completa
    int captures = 0;           // capturas de esta sesión
    int notice = 0;             // aviso en pantalla: 0 = ninguno, 1 = capturado, 2 = se ha escapado, 3 = sin unidades
    bool locked = false;        // cámara fijada a un objetivo
    const Inventory* inventory = nullptr; // inventario de pokéballs (válido durante el frame)

    // Pokémon al que se apunta dentro del rango de lanzamiento.
    bool hasAimTarget = false;
    int chancePercent = 0;      // porcentaje de captura (ya con bonificaciones)
    bool behind = false;        // bonificación por la espalda
    bool hidden = false;        // bonificación por sigilo (agachado)
};
