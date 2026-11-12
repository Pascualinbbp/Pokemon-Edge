#pragma once
#include <algorithm>
#include <map>
#include <vector>
#include "../models/gameData.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "daoRow.hpp"
#include "effectSpecDao.hpp"

// Movimientos (tabla move) con su clase (move_class), sus etiquetas (move_tag_link), sus etiquetas de aprendizaje (move_learn_tag) y su efecto (effect);
// y las etiquetas de aprendizaje de cada pokémon (pokemon_learn_tag).
class MoveDao {
    public:
    void load(GameData& data) const {
        std::map<int, MoveClass> classes;
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name FROM move_class;")) classes[DaoRow::integer(row, "id")] = { DaoRow::integer(row, "id"), DaoRow::text(row, "name"), MoveClass::parse(DaoRow::text(row, "name")) };
        for (const auto& row : SqliteUtil::executeSelect("SELECT id, name, priority FROM move_tag ORDER BY id;")) data.moveTags.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::integer(row, "priority") });

        for (const auto& row : SqliteUtil::executeSelect(std::string("SELECT m.id, m.name, m.description, m.type_id, m.class_id, m.pp, COALESCE(m.power, 0) AS power, COALESCE(m.accuracy, 0) AS accuracy, m.learn_weight, ") +
                                                         "COALESCE(e.kind, '') AS kind, COALESCE(e.param, '') AS param, COALESCE(e.target, 'FOE') AS target, COALESCE(e.amount, 0) AS amount, COALESCE(e.chance, 100) AS chance, COALESCE(e.multiplier, 1) AS multiplier "
                                                         "FROM move m LEFT JOIN effect e ON e.id = m.effect_id ORDER BY m.id;")) {
            Move move;
            move.id = DaoRow::integer(row, "id");
            move.name = DaoRow::text(row, "name");
            move.description = DaoRow::text(row, "description");
            move.typeId = DaoRow::integer(row, "type_id");
            move.classId = DaoRow::integer(row, "class_id");
            const auto cls = classes.find(move.classId);
            if (cls != classes.end()) move.kind = cls->second.kind;
            move.pp = (std::max)(1, DaoRow::integer(row, "pp"));
            move.power = DaoRow::integer(row, "power");
            move.accuracy = DaoRow::integer(row, "accuracy");
            move.learnWeight = (std::max)(0.0f, DaoRow::real(row, "learn_weight"));
            move.effect = EffectSpecDao::parse(row);
            data.moves.push_back(move);
        }

        for (const auto& row : SqliteUtil::executeSelect("SELECT move_id, tag_id FROM move_tag_link ORDER BY move_id, tag_id;")) {
            Move* move = find(data, DaoRow::integer(row, "move_id"));
            const MoveTag* tag = nullptr;
            for (const MoveTag& t : data.moveTags) if (t.id == DaoRow::integer(row, "tag_id")) tag = &t;
            if (!move || !tag) continue;
            move->tags.push_back(tag->id);
            move->tagNames.push_back(tag->name);
            move->priority += tag->priority;
            if (tag->name == "Contacto") move->contact = true;
            if (tag->name == "Todos los demás") move->scope = Move::Scope::ALL_OTHERS;
            else if (tag->name == "Ambos rivales") move->scope = Move::Scope::FOES;
            else if (tag->name == "Cualquier objetivo") move->scope = Move::Scope::ANY;
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT move_id, learn_tag_id FROM move_learn_tag;")) {
            if (Move* move = find(data, DaoRow::integer(row, "move_id"))) move->learnTags.push_back(DaoRow::integer(row, "learn_tag_id"));
        }
        for (const auto& row : SqliteUtil::executeSelect("SELECT pokemon_id, learn_tag_id FROM pokemon_learn_tag;")) {
            const int index = data.speciesIndex(DaoRow::integer(row, "pokemon_id"));
            if (index >= 0) data.species[static_cast<size_t>(index)].learnTags.push_back(DaoRow::integer(row, "learn_tag_id"));
        }
        if (data.moves.empty()) Logger::logError("MOVE_DAO", "La tabla move no existe o está vacía.");
    }

    private:
    static Move* find(GameData& data, int id) {
        for (Move& move : data.moves) if (move.id == id) return &move;
        return nullptr;
    }
};
