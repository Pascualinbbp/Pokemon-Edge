#pragma once
#include <string>
#include <vector>

// Hábitat (tabla habitat): cuántas zonas genera cada partida, de qué tamaño, y qué climas pone y cada cuánto.
struct WeatherChance {
    int weatherId = -1;
    float probability = 0.0f;
};

struct ResourceChance {
    int nodeId = -1;
    float weight = 0.0f;
};

// Dónde se coloca un hábitat al generar el mundo (columna placement).
enum class HabitatPlacement { ANY, COAST, BESIDE, SEA };

struct Habitat {
    int id = -1;
    std::string name;
    std::string description;
    int minZones = 1;
    int maxZones = 1;
    float minRadius = 10.0f;
    float maxRadius = 10.0f;
    float minStretch = 0.7f; // alargamiento de sus zonas (1 = redondas)
    float maxStretch = 1.5f;
    float minAltitude = 2.0f; // altura del terreno de sus zonas (m sobre el nivel del mar); para el mar, el rango de profundidades
    float maxAltitude = 4.0f;
    float roughness = 0.5f;   // ondulación del terreno (m)
    HabitatPlacement placement = HabitatPlacement::ANY;
    int besideId = -1;        // BESIDE: id del hábitat al que rodea
    int minChests = 0;       // cofres a la vez por zona (se sortea cada día)
    int maxChests = 2;
    int dailyChests = 2;     // tope de cofres que genera por día
    int minWild = 2;   // pokémon salvajes por chunk
    int maxWild = 5;
    int minNodes = 3;  // materiales de recolección por chunk
    int maxNodes = 6;
    bool sea() const { return placement == HabitatPlacement::SEA; }
    float minWeatherWait = 60.0f;
    float maxWeatherWait = 120.0f;
    std::vector<WeatherChance> weather;
    std::vector<ResourceChance> resources; // qué nodos de recolección hay y con qué peso
};
