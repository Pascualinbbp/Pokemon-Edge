#pragma once
#include <vector>
#include "../models/skill.hpp"
#include "../models/tool.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

class SkillDao {
    public:
    std::vector<Skill> findAll() const {
        std::vector<Skill> skills;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, description FROM skill ORDER BY id;")) {
            skills.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description") });
        }
        if (skills.empty()) Logger::logError("SKILL_DAO", "La tabla skill no existe o está vacía (falta aplicar testing/sql/skill.sql).");
        return skills;
    }

    // Herramientas con sus niveles y lo que cuesta subir a cada uno.
    std::vector<Tool> findTools() const {
        std::vector<Tool> tools;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, description, skill_id FROM tool ORDER BY id;")) {
            tools.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "description"),
                              DaoRow::integer(row, "skill_id"), {} });
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT tool_id, level, name, description, speed FROM tool_tier ORDER BY tool_id, level;")) {
            if (Tool* tool = find(tools, DaoRow::integer(row, "tool_id"))) {
                tool->tiers.push_back({ DaoRow::integer(row, "level"), DaoRow::text(row, "name"), DaoRow::text(row, "description"),
                                        DaoRow::real(row, "speed"), {} });
            }
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT tool_id, level, material_id, quantity FROM tool_upgrade ORDER BY tool_id, level;")) {
            Tool* tool = find(tools, DaoRow::integer(row, "tool_id"));
            if (!tool) continue;
            for (ToolTier& tier : tool->tiers) {
                if (tier.level == DaoRow::integer(row, "level")) tier.cost.push_back({ DaoRow::integer(row, "material_id"), DaoRow::integer(row, "quantity") });
            }
        }
        return tools;
    }

    private:
    static Tool* find(std::vector<Tool>& tools, int id) {
        for (Tool& tool : tools) if (tool.id == id) return &tool;
        return nullptr;
    }
};
