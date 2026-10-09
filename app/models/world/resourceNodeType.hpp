#pragma once
#include <string>

// Tipo de nodo de recolección (tabla resource_node): árbol, roca, mena...
// Tras 'hits' golpes da entre minYield y maxYield unidades de su material.
struct ResourceNodeType {
    int id = -1;
    std::string name;
    std::string action; // verbo de la ayuda: "Talar", "Picar"...
    int materialId = -1;
    int skillId = -1;   // habilidad necesaria para recolectarlo
    int level = 1;      // nivel mínimo de esa habilidad
    int hits = 1;
    int growSeconds = 0; // 0 = se extrae con su habilidad; mayor = planta cuyos frutos rebrotan solos en ese tiempo (regarla lo acelera)
    int regrowSeconds = 0; // solo si se extrae: tiempo que tarda en volver a estar listo tras agotarse
    bool plant() const { return growSeconds > 0; }
    int minYield = 1;
    int maxYield = 1;
};
