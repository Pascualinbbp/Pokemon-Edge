#pragma once
#include <string>
#include <vector>

// Hábitat (tabla habitat): cuántas zonas genera cada partida, de qué tamaño, y qué climas pone y cada cuánto.
struct WeatherChance {
    int weatherId = -1;
    float probability = 0.0f;
};

struct Habitat {
    int id = -1;
    std::string name;
    std::string description;
    int minZones = 1;
    int maxZones = 1;
    float minRadius = 10.0f;
    float maxRadius = 10.0f;
    int minWild = 2;   // pokémon salvajes por chunk
    int maxWild = 5;
    float minWeatherWait = 60.0f;
    float maxWeatherWait = 120.0f;
    std::vector<WeatherChance> weather;
};
