#pragma once
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "../../../models/gameData.hpp"

// Inventario del personaje: una ranura por objeto del juego (cualquier categoría) con sus unidades.
// Las pokéballs, además, forman la lista que se recorre al apuntar, con la ranura equipada.
class Inventory {
    public:
    static constexpr int STARTING_STOCK = 0; // unidades de cada objeto al empezar una partida (llegan en los cofres)
    // De las herramientas, 'count' es su nivel: todas empiezan en 1 (ver tool.sql).

    struct BallSlot {
        const PokeballType& type;
        int count;
    };

    void setData(const GameData& data) {
        m_data = &data;
        m_money = 0;
        m_slots.clear();
        m_balls.clear();
        m_slots.reserve(data.items.size());
        for (const Item& item : data.items) {
            if (item.category == ItemCategory::POKEBALL) {
                if (const PokeballType* type = data.ball(item.refId)) m_balls.push_back({ static_cast<int>(m_slots.size()), type });
            }
            m_slots.push_back({ &item, item.category == ItemCategory::TOOL ? 1 : STARTING_STOCK }); // herramientas: nivel 1
        }
        m_selected = 0;
    }

    // Suma 'quantity' unidades del objeto. Devuelve false si el objeto no existe.
    bool add(int itemId, int quantity) {
        for (Slot& slot : m_slots) {
            if (slot.item->id == itemId) {
                slot.count += quantity;
                return true;
            }
        }
        return false;
    }

    int count(int itemId) const {
        for (const Slot& slot : m_slots) if (slot.item->id == itemId) return slot.count;
        return 0;
    }

    // Recorre todos los objetos con sus unidades: f(const Item&, int count).
    template <typename F>
    void forEach(F f) const { for (const Slot& slot : m_slots) f(*slot.item, slot.count); }

    // --- Pokémonedas y venta ---
    int money() const { return m_money; }
    void addMoney(int amount) { m_money = (std::max)(0, m_money + amount); }

    // Vende hasta 'quantity' unidades de un objeto a su precio de venta (0 = no se vende). Devuelve lo cobrado.
    int sell(int itemId, int quantity) {
        const Item* item = m_data->item(itemId);
        if (!item || item->sellPrice <= 0) return 0;
        const int units = (std::min)(quantity, count(itemId));
        if (units <= 0) return 0;
        add(itemId, -units);
        m_money += units * item->sellPrice;
        return units * item->sellPrice;
    }

    // --- Herramientas (su nivel es el número de unidades de su ranura) ---
    // Mejor nivel de recolección que dan las herramientas en esta habilidad (0 = ninguna).
    int skillLevel(int skillId) const {
        int level = 0;
        for (const Slot& slot : m_slots) {
            if (const Tool* tool = m_data->toolOf(*slot.item); tool && tool->skillId == skillId) level = (std::max)(level, slot.count);
        }
        return level;
    }

    // Velocidad de trabajo del jugador con la mejor herramienta de esta habilidad (1 si no tiene).
    float skillSpeed(int skillId) const {
        float speed = 1.0f;
        int best = 0;
        for (const Slot& slot : m_slots) {
            const Tool* tool = m_data->toolOf(*slot.item);
            if (!tool || tool->skillId != skillId || slot.count <= best) continue;
            best = slot.count;
            if (const ToolTier* tier = tool->tier(slot.count)) speed = tier->speed;
        }
        return speed;
    }

    int toolLevel(const Tool& tool) const {
        const Item* item = m_data->toolItem(tool.id);
        return item ? count(item->id) : 0;
    }

    // Nivel siguiente de la herramienta (nullptr si ya está al máximo).
    const ToolTier* nextTier(const Tool& tool) const {
        const int level = toolLevel(tool);
        return level >= 1 && level < tool.maxLevel() ? &tool.tiers[level] : nullptr;
    }

    bool canUpgrade(const Tool& tool) const {
        const ToolTier* next = nextTier(tool);
        if (!next) return false;
        for (const RecipeIngredient& ingredient : next->cost) {
            const Item* item = m_data->materialItem(ingredient.materialId);
            if (!item || count(item->id) < ingredient.quantity) return false;
        }
        return true;
    }

    // Gasta los materiales y sube la herramienta un nivel. Devuelve false si no se puede.
    bool upgrade(const Tool& tool) {
        if (!canUpgrade(tool)) return false;
        for (const RecipeIngredient& ingredient : nextTier(tool)->cost) add(m_data->materialItem(ingredient.materialId)->id, -ingredient.quantity);
        return add(m_data->toolItem(tool.id)->id, 1);
    }

    // --- Pokéballs ---
    bool hasBalls() const { return !m_balls.empty(); }
    int ballCount() const { return static_cast<int>(m_balls.size()); }
    int selectedIndex() const { return m_selected; }
    BallSlot ball(int index) const { return { *m_balls[index].type, m_slots[m_balls[index].slot].count }; }
    BallSlot selectedBall() const { return ball(m_selected); }
    float captureMultiplier() const { return hasBalls() ? selectedBall().type.captureMultiplier : 1.0f; }

    bool canThrow() const { return hasBalls() && selectedBall().count > 0; }
    void consume() { if (canThrow()) --m_slots[m_balls[m_selected].slot].count; }

    // direction: +1 siguiente, -1 anterior (circular).
    void cycle(int direction) {
        const int n = ballCount();
        if (n > 1) m_selected = ((m_selected + direction) % n + n) % n;
    }

    // --- Guardado (por nombre de objeto) ---
    void store(std::map<std::string, int>& counts, std::string& selectedName, int& money) const {
        money = m_money;
        counts.clear();
        for (const Slot& slot : m_slots) counts[m_data->itemName(*slot.item)] = slot.count;
        selectedName = hasBalls() ? m_data->itemName(*m_slots[m_balls[m_selected].slot].item) : std::string();
    }

    // Los objetos que no aparecen en el guardado conservan sus unidades iniciales.
    void restore(const std::map<std::string, int>& counts, const std::string& selectedName, int money) {
        m_money = (std::max)(money, 0);
        for (Slot& slot : m_slots) {
            if (const auto it = counts.find(m_data->itemName(*slot.item)); it != counts.end()) slot.count = (std::max)(it->second, slot.item->category == ItemCategory::TOOL ? 1 : 0);
        }
        for (int i = 0; i < ballCount(); ++i) if (m_data->itemName(*m_slots[m_balls[i].slot].item) == selectedName) m_selected = i;
    }

    private:
    struct Slot {
        const Item* item;
        int count;
    };
    struct BallRef {
        int slot; // índice en m_slots
        const PokeballType* type;
    };

    const GameData* m_data = &GameData::empty();
    std::vector<Slot> m_slots;
    std::vector<BallRef> m_balls;
    int m_selected = 0;
    int m_money = 0; // pokémonedas
};
