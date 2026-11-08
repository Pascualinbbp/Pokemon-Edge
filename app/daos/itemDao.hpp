#pragma once
#include <vector>
#include "../models/gameData.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "../utils/core/stringUtil.hpp"
#include "daoRow.hpp"

// Objetos (tabla item + su efecto + su categoría). Cada fila se reparte según el efecto: CAPTURE -> pokéball, HEAL / REVIVE / LEVEL / EV ->
// objeto de entrenamiento, EVOLVE -> piedra evolutiva. Los materiales y las herramientas también entran en el inventario como objetos.
class ItemDao {
    public:
    void load(GameData& data) const {
        for (const auto& row : SqliteUtil::executeSelect(SELECT_CATEGORIES)) {
            data.categories.push_back({ DaoRow::integer(row, "id"), DaoRow::text(row, "name"), DaoRow::text(row, "section").empty() ? "Otros" : DaoRow::text(row, "section"), -1 });
        }
        for (const auto& row : SqliteUtil::executeSelect(SELECT_ITEMS)) addItem(data, row);
        if (data.items.empty()) Logger::logError("ITEM_DAO", "Las tablas de objetos no existen o están vacías.");
        for (const Material& material : data.materials) {
            data.items.push_back({ MATERIAL_ITEM_BASE + material.id, ItemCategory::MATERIAL, material.categoryId, material.id, -1, material.name, material.description, "",
                                   material.buyPrice, material.sellPrice, 0, -1 });
        }
        for (const Tool& tool : data.tools) {
            data.items.push_back({ TOOL_ITEM_BASE + tool.id, ItemCategory::TOOL, tool.categoryId, tool.id, -1, tool.name, tool.description, "", 0, 0, 0, -1 });
        }
        for (ItemCategoryInfo& category : data.categories) {
            for (const Item& item : data.items) if (item.categoryId == category.id) { category.iconItemId = item.id; break; }
        }
    }

    private:
    static void addItem(GameData& data, const SqliteUtil::Row& row) {
        const int id = DaoRow::integer(row, "id");
        const std::string& kind = DaoRow::text(row, "kind");
        const std::string asset = "items/" + StringUtil::slug(DaoRow::text(row, "category")) + "/" + std::to_string(id);
        if (kind == "CAPTURE" || kind == "CAPTURE_EXCLUSIVE") {
            data.balls.push_back({ id, DaoRow::text(row, "name"), DaoRow::text(row, "description"), DaoRow::real(row, "multiplier"), DaoRow::real(row, "shiny_multiplier"),
                                   kind == "CAPTURE", asset });
            if (kind == "CAPTURE_EXCLUSIVE") return; // la bola del pokémon inicial no es un objeto
        }
        Item item{ id, ItemCategory::UNKNOWN, DaoRow::integer(row, "category_id"), id, -1, DaoRow::text(row, "name"), DaoRow::text(row, "description"), asset,
                   DaoRow::integer(row, "buy_price"), DaoRow::integer(row, "sell_price"), DaoRow::integer(row, "gift_every"),
                   DaoRow::text(row, "gift_reward_id").empty() ? -1 : DaoRow::integer(row, "gift_reward_id") };
        if (kind == "CAPTURE") item.category = ItemCategory::POKEBALL;
        else if (kind == "EVOLVE") { item.category = ItemCategory::STONE; item.typeId = DaoRow::integer(row, "type_id"); }
        else if (const HeldItem::Effect* held = heldEffect(kind)) {
            item.category = ItemCategory::HELD;
            data.heldItems.push_back({ id, item.name, item.description, *held, DaoRow::integer(row, "amount"), DaoRow::rgb(row, "color") });
        }
        else if (const TrainingItem::Effect* effect = trainingEffect(kind)) {
            item.category = ItemCategory::TRAINING;
            data.trainingItems.push_back({ id, item.name, item.description, *effect, asset, DaoRow::rgb(row, "color"), DaoRow::integer(row, "amount"), kind == "HEAL_PERCENT" });
        }
        data.items.push_back(std::move(item));
    }

    static const HeldItem::Effect* heldEffect(const std::string& kind) {
        using E = HeldItem::Effect;
        static const E SHARE = E::EXP_SHARE, QUICK = E::QUICK_CLAW, RAZOR = E::RAZOR_CLAW, BAND = E::FOCUS_BAND, LEFT = E::LEFTOVERS, ORB = E::LIFE_ORB;
        return kind == "HOLD_EXP_SHARE" ? &SHARE : kind == "HOLD_QUICK_CLAW" ? &QUICK : kind == "HOLD_RAZOR_CLAW" ? &RAZOR : kind == "HOLD_FOCUS_BAND" ? &BAND :
               kind == "HOLD_LEFTOVERS" ? &LEFT : kind == "HOLD_LIFE_ORB" ? &ORB : nullptr;
    }

    static const TrainingItem::Effect* trainingEffect(const std::string& kind) {
        static const TrainingItem::Effect LEVEL = TrainingItem::Effect::LEVEL, EV = TrainingItem::Effect::EV, HEAL = TrainingItem::Effect::HEAL, REVIVE = TrainingItem::Effect::REVIVE;
        return kind == "LEVEL" ? &LEVEL : kind == "EV" ? &EV : kind == "HEAL" || kind == "HEAL_PERCENT" ? &HEAL : kind == "REVIVE" ? &REVIVE : nullptr;
    }

    static constexpr const char* SELECT_ITEMS =
        "SELECT i.id, i.name, i.description, i.category_id, c.name AS category, i.buy_price, i.sell_price, i.gift_every, i.gift_reward_id, "
        "e.kind, e.type_id, e.multiplier, e.shiny_multiplier, e.amount, e.color "
        "FROM item i JOIN category c ON c.id = i.category_id LEFT JOIN effect e ON e.id = i.effect_id ORDER BY i.id;";
    static constexpr const char* SELECT_CATEGORIES = "SELECT id, name, section FROM category ORDER BY id;";
};
