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
// ANY / COAST: zonas de tierra; BESIDE: orilla de un hábitat de agua (besideId); SEA: mar por profundidad; RIVER / UPPER_RIVER / LAKE /
// UPPER_LAKE / POND: agua dulce (UPPER = aguas arriba de una cascada; POND = charcas de un pantano, besideId); PEAK: tierra por encima de
// la línea de nieve; CAVE / MINE: sistemas de galerías BAJO el terreno a los que se entra andando por una boca (muchos, de varios tamaños);
// HALL: tipo de sala dentro de una cueva o mina (besideId = el hábitat de cueva o mina al que pertenece; -1 = cualquiera);
// UNDERGROUND_WATER: agua de las grutas; SKY: suelo de nubes de las islas del cielo; SKY_LAND: parcelas de tierra sobre las nubes; SKY_SEA: mar de nubes.
enum class HabitatPlacement { ANY, COAST, BESIDE, SEA, RIVER, UPPER_RIVER, LAKE, UPPER_LAKE, POND, PEAK, CAVE, MINE, HALL, UNDERGROUND_WATER, SKY, SKY_LAND, SKY_SEA };

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
    bool land() const { return placement == HabitatPlacement::ANY || placement == HabitatPlacement::COAST; }
    // Agua dulce: su min/max_altitude son la profundidad (negativa) de ríos, lagos y charcas.
    bool freshWater() const {
        return placement == HabitatPlacement::RIVER || placement == HabitatPlacement::UPPER_RIVER || placement == HabitatPlacement::LAKE ||
               placement == HabitatPlacement::UPPER_LAKE || placement == HabitatPlacement::POND;
    }
    float minWeatherWait = 60.0f;
    float maxWeatherWait = 120.0f;
    std::vector<WeatherChance> weather;
    std::vector<ResourceChance> resources; // qué nodos de recolección hay y con qué peso
};
