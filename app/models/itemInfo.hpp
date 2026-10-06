#pragma once
#include <cstdio>
#include <string>
#include <vector>
#include "gameData.hpp"

// Información derivada de las tablas de la base de datos para mostrar de un objeto: de dónde sale y para qué sirve.
namespace ItemInfo {
    // Probabilidad (%) de que un cofre dé este objeto al abrirse.
    inline float chestChance(const ChestType& chest, int itemId) {
        float total = 0.0f, mine = 0.0f;
        for (const ChestReward& reward : chest.rewards) {
            total += reward.probability;
            if (reward.itemId == itemId) mine += reward.probability;
        }
        return total > 0.0f ? mine / total * 100.0f : 0.0f;
    }

    // Cómo conseguirlo: nodos del mundo que lo dan, cofres que lo contienen o su fabricación.
    inline std::vector<std::string> sources(const GameData& data, const Item& item) {
        std::vector<std::string> lines;
        char text[128];
        if (item.category == ItemCategory::MATERIAL) {
            for (const ResourceNodeType& node : data.nodes) {
                if (node.materialId != item.refId) continue;
                if (node.plant()) std::snprintf(text, sizeof(text), "%s: %s", node.action.c_str(), node.name.c_str());
                else std::snprintf(text, sizeof(text), "%s: %s (nivel %d)", node.action.c_str(), node.name.c_str(), node.level);
                lines.emplace_back(text);
            }
        } else if (item.category == ItemCategory::TOOL) {
            lines.emplace_back("La tienes desde el inicio");
        }
        for (const GroundItemType& ground : data.groundItems) {
            if (ground.itemId == item.id) lines.emplace_back("Suelto por el mundo");
        }
        for (const ChestType& chest : data.chests) {
            if (const float chance = chestChance(chest, item.id); chance > 0.0f) {
                std::snprintf(text, sizeof(text), "%s (%.0f %%)", chest.name.c_str(), chance);
                lines.emplace_back(text);
            }
        }
        return lines;
    }

    // Para qué sirve: mejoras de herramientas que lo piden.
    inline std::vector<std::string> uses(const GameData& data, const Item& item) {
        std::vector<std::string> lines;
        if (item.category != ItemCategory::MATERIAL) return lines;
        char text[128];
        for (const Tool& tool : data.tools) {
            for (const ToolTier& tier : tool.tiers) {
                for (const RecipeIngredient& ingredient : tier.cost) {
                    if (ingredient.materialId != item.refId) continue;
                    std::snprintf(text, sizeof(text), "Mejorar a %s (x%d)", tier.name.c_str(), ingredient.quantity);
                    lines.emplace_back(text);
                }
            }
        }
        return lines;
    }
}
