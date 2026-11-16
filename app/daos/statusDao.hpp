#pragma once
#include <vector>
#include "../models/pokemon/statusInfo.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"

// Estados alterados (tabla status): el efecto de cada uno (tabla effect, param = su código) dice qué estado es.
class StatusDao {
    public:
    std::vector<StatusInfo> findAll() const {
        std::vector<StatusInfo> list;
        for (const auto& row : SqliteUtil::executeSelect("SELECT s.id, s.name, s.type_id, COALESCE(e.param, '') AS param FROM status s LEFT JOIN effect e ON e.id = s.effect_id ORDER BY s.id;")) {
            StatusInfo info;
            info.id = DaoRow::integer(row, "id");
            info.name = DaoRow::text(row, "name");
            info.typeId = DaoRow::integer(row, "type_id");
            info.code = EffectSpec::parseStatus(DaoRow::text(row, "param"));
            if (info.code != EffectSpec::Status::NONE) list.push_back(info);
        }
        if (list.empty()) Logger::logError("STATUS_DAO", "La tabla status no existe o está vacía.");
        return list;
    }
};
