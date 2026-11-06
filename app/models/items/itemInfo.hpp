#pragma once
#include <cstdio>
#include <string>
#include <vector>
#include "../gameData.hpp"

// Información derivada de las tablas de la base de datos para mostrar de un objeto: de dónde sale y para qué sirve.
namespace ItemInfo {
    // Probabilidad (%) de que el grupo de recompensas dé este objeto.
    inline float groupChance(const GameData& data, int groupId, const Item& item) {
        const auto range = data.rewardGroup(groupId);
        float total = 0.0f, mine = 0.0f;
        for (const RewardEntry* e = range.first; e != range.second; ++e) {
            total += e->probability;
            if (const Item* found = data.rewardItem(*e); found && found->id == item.id) mine += e->probability;
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
        if (const float chance = groupChance(data, RewardGroup::GROUND, item); chance > 0.0f) lines.emplace_back("Suelto por el mundo");
        for (const ChestType& chest : data.chests) {
            if (const float chance = groupChance(data, chest.rewardId, item); chance > 0.0f) {
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
                    std::snprintf(text, sizeof(text), "Mejorar %s (x%d)", tool.title(tier.level).c_str(), ingredient.quantity);
                    lines.emplace_back(text);
                }
            }
        }
        return lines;
    }
}
