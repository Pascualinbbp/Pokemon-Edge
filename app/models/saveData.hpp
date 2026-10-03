#pragma once
#include <array>

// Datos persistidos de una partida. Se irá ampliando a medida que avance el juego.
struct SaveData {
    std::array<float, 3> playerPosition = { 0.0f, 0.0f, 0.0f };
};