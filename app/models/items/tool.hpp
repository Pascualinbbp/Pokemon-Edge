#pragma once
#include <string>
#include <vector>

// Material necesario para mejorar una herramienta (tabla tool_upgrade).
struct RecipeIngredient {
    int materialId = -1;
    int quantity = 1;
};

// Una herramienta a un nivel (tabla tool_tier): nombre, descripción, velocidad de trabajo y lo que cuesta alcanzarlo.
struct ToolTier {
    int level = 1;
    std::string name;
    std::string description;
    float speed = 1.0f;
    std::vector<RecipeIngredient> cost; // vacío en el nivel inicial
};

// Herramienta (tabla tool): el jugador las tiene siempre. Su nivel (guardado como unidades en el inventario) es
// también su nivel de recolección en 'skillId'.
struct Tool {
    int id = -1;
    std::string name;
    std::string description;
    int skillId = -1;
    std::vector<ToolTier> tiers; // ordenados por nivel (el índice 0 es el nivel 1)

    int maxLevel() const { return static_cast<int>(tiers.size()); }

    // Datos del nivel dado (se ajusta al rango válido). nullptr si no tiene niveles.
    const ToolTier* tier(int level) const {
        if (tiers.empty()) return nullptr;
        const int index = level < 1 ? 0 : level > maxLevel() ? maxLevel() - 1 : level - 1;
        return &tiers[index];
    }
};
