#pragma once
#include <map>
#include <string>
#include <vector>
#include "../../models/gameData.hpp"

// Inventario del personaje: una ranura por objeto del juego (cualquier categoría) con sus unidades.
// Las pokéballs, además, forman la lista que se recorre al apuntar, con la ranura equipada.
class Inventory {
    public:
    static constexpr int STARTING_STOCK = 0; // unidades de cada objeto al empezar una partida (llegan en los cofres)

    struct BallSlot {
        const PokeballType& type;
        int count;
    };

    void setData(const GameData& data) {
        m_slots.clear();
        m_balls.clear();
        m_slots.reserve(data.items.size());
        for (const Item& item : data.items) {
            if (item.category == ItemCategory::POKEBALL) {
                if (const PokeballType* type = data.ball(item.refId)) m_balls.push_back({ static_cast<int>(m_slots.size()), type });
            }
            m_slots.push_back({ &item, STARTING_STOCK });
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
    void store(std::map<std::string, int>& counts, std::string& selectedName) const {
        counts.clear();
        for (const Slot& slot : m_slots) counts[slot.item->name] = slot.count;
        selectedName = hasBalls() ? m_slots[m_balls[m_selected].slot].item->name : std::string();
    }

    // Los objetos que no aparecen en el guardado conservan sus unidades iniciales.
    void restore(const std::map<std::string, int>& counts, const std::string& selectedName) {
        for (Slot& slot : m_slots) {
            if (const auto it = counts.find(slot.item->name); it != counts.end()) slot.count = it->second < 0 ? 0 : it->second;
        }
        for (int i = 0; i < ballCount(); ++i) if (m_slots[m_balls[i].slot].item->name == selectedName) m_selected = i;
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

    std::vector<Slot> m_slots;
    std::vector<BallRef> m_balls;
    int m_selected = 0;
};
