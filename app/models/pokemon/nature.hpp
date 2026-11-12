#pragma once
#include <string>

// Naturaleza de un pokémon (tabla nature): sube un 10 % una estadística y baja otro 10 % otra (-1 = ninguna: naturaleza neutra).
// Los índices son los de BaseStats: 0 PS, 1 ataque, 2 ataque especial, 3 defensa, 4 defensa especial, 5 velocidad (los PS nunca cambian).
struct Nature {
    int id = -1;
    std::string name;
    int increased = -1;
    int decreased = -1;

    bool neutral() const { return increased < 0 || decreased < 0 || increased == decreased; }
};
