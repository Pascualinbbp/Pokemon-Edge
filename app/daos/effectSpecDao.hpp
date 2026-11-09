#pragma once
#include "../models/pokemon/effectSpec.hpp"
#include "daoRow.hpp"

// Efecto de combate a partir de una fila que trae las columnas de effect: kind, param, target, amount, chance, multiplier (ver EffectColumns).
namespace EffectSpecDao {
    inline constexpr const char* COLUMNS = "e.kind AS kind, COALESCE(e.param, '') AS param, COALESCE(e.target, 'FOE') AS target, COALESCE(e.amount, 0) AS amount, COALESCE(e.chance, 100) AS chance, COALESCE(e.multiplier, 1) AS multiplier";

    inline EffectSpec parse(const SqliteUtil::Row& row) {
        EffectSpec spec;
        spec.kind = EffectSpec::parseKind(DaoRow::text(row, "kind"));
        if (!spec.active()) return spec;
        const std::string& param = DaoRow::text(row, "param");
        if (spec.kind == EffectSpec::Kind::STAT_STAGE) spec.stat = EffectSpec::parseStat(param);
        if (spec.kind == EffectSpec::Kind::STATUS) spec.status = EffectSpec::parseStatus(param);
        spec.self = DaoRow::text(row, "target") == "SELF";
        spec.amount = DaoRow::real(row, "amount");
        spec.chance = DaoRow::real(row, "chance");
        spec.multiplier = DaoRow::real(row, "multiplier");
        return spec;
    }
}
