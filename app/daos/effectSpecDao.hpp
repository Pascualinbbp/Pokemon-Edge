#pragma once
#include "../models/pokemon/effectSpec.hpp"
#include "daoRow.hpp"

// Efecto de combate a partir de una fila que trae las columnas de effect (ver COLUMNS) y, si quien lo usa tiene probabilidad propia, la columna 'chance'
// (move.effect_chance / ability.effect_chance; sin ella, 100).
namespace EffectSpecDao {
    inline constexpr const char* COLUMNS = "e.kind AS kind, COALESCE(e.param, '') AS param, COALESCE(e.target, 'FOE') AS target, COALESCE(e.amount, 0) AS amount, COALESCE(e.multiplier, 1) AS multiplier, COALESCE(e.condition, '') AS condition, COALESCE(e.type_id, 0) AS spec_type";

    inline EffectSpec parse(const SqliteUtil::Row& row, float chance = 100.0f) {
        EffectSpec spec;
        spec.kind = EffectSpec::parseKind(DaoRow::text(row, "kind"));
        if (!spec.active()) return spec;
        const std::string& param = DaoRow::text(row, "param");
        if (spec.kind == EffectSpec::Kind::STAT_STAGE || spec.kind == EffectSpec::Kind::STAT_MULT || spec.kind == EffectSpec::Kind::ABSORB_TYPE) spec.stat = EffectSpec::parseStat(param);
        if (spec.kind == EffectSpec::Kind::STATUS) spec.status = EffectSpec::parseStatus(param);
        if (spec.kind == EffectSpec::Kind::WEATHER) spec.weather = EffectSpec::parseWeather(param);
        spec.param = param;
        spec.cond = EffectSpec::parseCond(DaoRow::text(row, "condition"));
        spec.self = DaoRow::text(row, "target") == "SELF";
        spec.amount = DaoRow::real(row, "amount");
        spec.chance = chance;
        spec.multiplier = DaoRow::real(row, "multiplier");
        spec.typeId = DaoRow::integer(row, "spec_type") > 0 ? DaoRow::integer(row, "spec_type") : -1;
        return spec;
    }
}
