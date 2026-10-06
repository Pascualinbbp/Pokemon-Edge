#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../../models/gameData.hpp"
#include "../world/inventory.hpp"

// Avisos grandes en pantalla.
enum class Notice { NONE, CAPTURED, ESCAPED, LUCKY, SUPER_LUCKY, OUT_OF_STOCK, REWARD };

// Nombre que se dibuja sobre un pokémon (posición en el mundo, sobre su cabeza).
struct NameTag {
    DirectX::XMFLOAT3 position;
    const std::string* name;
};

// Estado del juego que la interfaz necesita mostrar (el HUD no conoce la escena).
struct GameStatus {
    float aimBlend = 0.0f;      // 0 = cámara normal, 1 = cámara de apuntado completa
    int captures = 0;           // capturas de esta sesión
    Notice notice = Notice::NONE;
    bool locked = false;        // cámara fijada a un objetivo
    const Inventory* inventory = nullptr; // inventario (válido durante el frame)
    const GameData* data = nullptr;              // datos de juego (nombres de objetos...)
    bool aiming = false;                         // modo captura activo
    bool canLock = false;                        // hay un pokémon al que fijar la cámara
    const char* interactVerb = nullptr;          // acción disponible junto a un cofre o recurso ("Abrir", "Talar"...)
    const std::string* interactTarget = nullptr; // sobre qué ("Cofre común", "Árbol"...)
    const std::string* missingSkill = nullptr;   // habilidad que falta para trabajar el recurso cercano ("Talar"...)
    int missingLevel = 0;                        // nivel que exige ese recurso
    std::vector<NameTag> nameTags;               // nombres sobre los pokémon visibles
    DirectX::XMFLOAT4X4 viewProj = {};           // vista * proyección (para colocar los nombres en pantalla)
    const std::string* noticeText = nullptr;     // segunda línea del aviso (recompensa obtenida, pokémon capturado...)

    // Pokémon al que se apunta dentro del rango de lanzamiento.
    bool hasAimTarget = false;
    int chancePercent = 0;      // porcentaje de captura (ya con bonificaciones)
    bool behind = false;        // bonificación por la espalda
    bool hidden = false;        // bonificación por sigilo (agachado)
};
