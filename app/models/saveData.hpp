#pragma once
#include <array>
#include <string>

// Datos persistidos de una partida. Se irá ampliando a medida que avance el juego.
struct SaveData {
    std::array<float, 3> playerPosition = { 0.0f, 0.0f, 0.0f };
    float worldTime = -1.0f; // segundos dentro del ciclo día/noche; negativo = sin dato (se usa el inicio por defecto)
};

// Resumen de una ranura de partida para mostrarla en los menús.
struct SaveSlotInfo {
    bool used = false;
    std::string savedAtText; // fecha y hora ya formateadas (se calculan al guardar, no en cada frame)
};
