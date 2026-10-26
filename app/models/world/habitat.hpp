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
// UNDERGROUND_WATER: agua de las grutas; SKY: suelo de nubes de las islas del cielo; SKY_LAND: parcelas de tierra sobre las nubes; SKY_SEA: mar de nubes; SKY_CAVE: cuevas excavadas en colinas del suelo de nube.
// VOLCANO: volcán de tierra (cono con cráter de lava); LAVA: lava (cráter y ríos de lava); GEYSER: géiseres (parcelas con varias charcas junto al hábitat besideId); HOT_SPRING: aguas termales (charcas de agua caliente alrededor de los géiseres de besideId);
// SEA_VOLCANO: volcán submarino; VENT: fumarolas en el cráter de un volcán submarino; REEF: arrecife de coral; KELP: campo de algas;
// GLACIER: glaciar de hielo. CAVE / MINE con besideId = galerías cuya boca se abre en las laderas de ese hábitat (gruta volcánica, gruta de hielo).
enum class HabitatPlacement { ANY, COAST, BESIDE, SEA, RIVER, UPPER_RIVER, LAKE, UPPER_LAKE, POND, PEAK, CAVE, MINE, HALL, UNDERGROUND_WATER, SKY, SKY_LAND, SKY_SEA, SKY_CAVE,
                              VOLCANO, LAVA, GEYSER, HOT_SPRING, SEA_VOLCANO, VENT, REEF, KELP, GLACIER, VOLCANIC_ROCK };

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
    int blendA = -1;              int blendB = -1;
    float warmth = 0.0f;           // °C que suma a la temperatura de la zona (volcán y aguas termales calientan; el hielo enfría)
    float altitudeCooling = 1.0f;  // cuánto enfría la altura aquí (1 = normal; en un volcán casi nada, el calor del magma lo compensa)
    float friction = 1.0f;    // agarre del suelo: 1 = normal; el hielo resbala (cerca de 0)
    int minChests = 0;       // cofres a la vez por zona (se sortea cada día)
    int maxChests = 2;
    int dailyChests = 2;     // tope de cofres que genera por día
    int minWild = 2;   // pokémon salvajes por chunk
    int maxWild = 5;
    int minNodes = 3;  // materiales de recolección por chunk
    int maxNodes = 6;
    bool sea() const { return placement == HabitatPlacement::SEA; }
    bool land() const {
        return placement == HabitatPlacement::ANY || placement == HabitatPlacement::COAST || placement == HabitatPlacement::VOLCANO || placement == HabitatPlacement::GLACIER;
    }
    // Zonas bajo el mar (se elevan desde el fondo): volcanes submarinos, arrecifes y campos de algas.
    bool seabed() const { return placement == HabitatPlacement::SEA_VOLCANO || placement == HabitatPlacement::REEF || placement == HabitatPlacement::KELP; }
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
