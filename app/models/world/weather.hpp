#pragma once
#include <string>

// Clima (tabla weather) y la fusión de dos climas en uno nuevo (tabla weather_fusion).
struct Weather {
    int id = -1;
    std::string name;
    std::string description;
};

struct WeatherFusion {
    int a = -1;
    int b = -1;
    int result = -1;
};
