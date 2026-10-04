#pragma once
#include <array>
#include <map>
#include <string>

// Datos persistidos de una partida. Se irá ampliando a medida que avance el juego.
struct SaveData {
    std::array<float, 3> playerPosition = { 0.0f, 0.0f, 0.0f };
    float worldTime = -1.0f; // segundos dentro del ciclo día/noche; negativo = sin dato (se usa el inicio por defecto)
    std::map<std::string, int> balls; // unidades de cada tipo de pokéball por nombre; vacío = valores iniciales
    std::string selectedBall;         // pokéball equipada; vacío = la primera
};

// Resumen de una ranura de partida para mostrarla en los menús.
struct SaveSlotInfo {
    bool used = false;
    std::string savedAtText; // fecha y hora ya formateadas (se calculan al guardar, no en cada frame)
};
