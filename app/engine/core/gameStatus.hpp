#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../../models/gameData.hpp"
#include "../world/habitat/habitatMap.hpp"
#include "../world/state/exploration.hpp"
#include "../world/state/inventory.hpp"

// Avisos grandes en pantalla.
enum class Notice { NONE, CAPTURED, ESCAPED, LUCKY, SUPER_LUCKY, OUT_OF_STOCK, REWARD, LEVEL_UP };

// Nombre que se dibuja sobre un pokémon (posición en el mundo, sobre su cabeza).
struct NameTag {
    DirectX::XMFLOAT3 position;
    std::string text; // "Nombre  Nv. 12"
    bool shiny = false;
};

// Un pokémon del equipo para la lista lateral del juego.
struct TeamEntry {
    const PokemonSpecies* species = nullptr;
    int level = 1;
    bool shiny = false;
    int ballId = -1;     // pokéball con la que se capturó
    bool active = false; // el que acompaña al jugador
};

// Estado del juego que la interfaz necesita mostrar (el HUD no conoce la escena).
struct GameStatus {
    float aimBlend = 0.0f;      // 0 = cámara normal, 1 = cámara de apuntado completa
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
    DirectX::XMFLOAT3 interactPos = {};          // dónde se dibuja la ayuda de interacción (sobre lo que se usa)
    DirectX::XMFLOAT3 missingPos = {};           // y el aviso de lo que falta (sobre el recurso)
    const HabitatMap* habitatMap = nullptr;      // mapa de hábitats (para el minimapa)
    float playerX = 0.0f, playerZ = 0.0f, playerYaw = 0.0f;
    const Exploration* exploration = nullptr;    // lo explorado (mapa grande)
    std::vector<DirectX::XMFLOAT4> mapBlocks;    // construcciones fijas (x, z, semilado x, semilado z) para los mapas
    const std::string* habitatName = nullptr;    // hábitat dominante donde está el jugador
    const std::string* weatherName = nullptr;    // clima más fuerte donde está el jugador (nullptr = despejado)
    float dayAngle = 0.0f;                       // posición del sol: 0..π de día, π..2π de noche
    const char* companionMode = nullptr;         // modo del pokémon que acompaña (nullptr = no hay)
    std::vector<NameTag> nameTags;               // nombres sobre los pokémon visibles
    std::vector<TeamEntry> team;                 // el equipo, para la lista lateral
    int playerLevel = 1;                         // nivel del jugador, su experiencia (0..1) y su límite de nivel
    float playerXp = 0.0f;
    int levelCap = 1;
    DirectX::XMFLOAT4X4 viewProj = {};           // vista * proyección (para colocar los nombres en pantalla)
    const std::string* noticeText = nullptr;     // segunda línea del aviso (recompensa obtenida, pokémon capturado...)

    // Pokémon al que se apunta dentro del rango de lanzamiento.
    bool hasAimTarget = false;
    int chancePercent = 0;      // porcentaje de captura (ya con bonificaciones)
    bool behind = false;        // bonificación por la espalda
    bool hidden = false;        // bonificación por sigilo (agachado)
};
