#pragma once
#include <string>

// Tipo de nodo de recolección (tabla resource_node): árbol, roca, mena...
// Tras 'hits' golpes da entre minYield y maxYield unidades de su material.
struct ResourceNodeType {
    int id = -1;
    std::string name;
    std::string action; // verbo de la ayuda: "Talar", "Picar"...
    int materialId = -1;
    int hits = 1;
    int minYield = 1;
    int maxYield = 1;
    float spawnWeight = 1.0f;
};
