#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

// Datos persistidos de una partida. Se irá ampliando a medida que avance el juego.
struct SaveData {
    std::array<float, 3> playerPosition = { 0.0f, 0.0f, 0.0f };
    float worldTime = -1.0f; // segundos dentro del ciclo día/noche; negativo = sin dato (se usa el inicio por defecto)
    std::map<std::string, int> items; // unidades de cada objeto por nombre; vacío = valores iniciales
    std::string selectedBall;         // pokéball equipada; vacío = la primera
    std::vector<std::string> team;    // especies del equipo (el primero es el líder)
    std::vector<std::string> pc;      // especies guardadas en el PC
};

// Resumen de una ranura de partida para mostrarla en los menús.
struct SaveSlotInfo {
    bool used = false;
    std::string savedAtText; // fecha y hora ya formateadas (se calculan al guardar, no en cada frame)
};
