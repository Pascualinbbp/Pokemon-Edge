#pragma once
#include <string>
#include <vector>

// Material necesario para mejorar una herramienta (tabla tool_upgrade).
struct RecipeIngredient {
    int materialId = -1;
    int quantity = 1;
};

// Lo que añade una herramienta al subir a un nivel (tabla tool_level) y lo que cuesta alcanzarlo. Los efectos se acumulan:
// a nivel N tiene lo de todos los niveles hasta N.
struct ToolTier {
    int level = 1;
    std::string description;            // lo que añade este nivel a los anteriores
    float speedBonus = 0.0f;            // se suma a la velocidad de trabajo base (1.0)
    std::vector<RecipeIngredient> cost; // vacío en el nivel inicial
};

// Herramienta (tabla tool): el jugador las tiene siempre. Su nivel (guardado como unidades en el inventario) es
// también su nivel de recolección en 'skillId'.
struct Tool {
    static constexpr float BASE_SPEED = 1.0f;

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

    // Velocidad de trabajo a ese nivel: la base más las bonificaciones de todos los niveles hasta él.
    float speed(int level) const {
        float total = BASE_SPEED;
        for (const ToolTier& entry : tiers) if (entry.level <= level) total += entry.speedBonus;
        return total;
    }

    std::string title(int level) const { return name + " Nv. " + std::to_string(level < 1 ? 1 : level > maxLevel() ? maxLevel() : level); }

    // Lo que hace a ese nivel: el efecto de cada nivel hasta él, uno por línea.
    std::string effects(int level) const {
        std::string text;
        for (const ToolTier& entry : tiers) {
            if (entry.level > level) break;
            text += (text.empty() ? "" : "\n") + std::string("Nv. ") + std::to_string(entry.level) + ": " + entry.description;
        }
        return text;
    }
};
