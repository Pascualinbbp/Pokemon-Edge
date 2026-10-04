#pragma once
#include <map>
#include <string>
#include <vector>
#include "../../models/pokeballType.hpp"

// Inventario de pokéballs del personaje: una ranura por tipo, con su cantidad, y la ranura equipada.
class Inventory {
    public:
    struct Slot {
        PokeballType type;
        int count = 0;
    };

    void setTypes(const std::vector<PokeballType>& types) {
        m_slots.clear();
        m_slots.reserve(types.size());
        for (const PokeballType& type : types) m_slots.push_back({ type, type.startingStock });
        m_selected = 0;
    }

    bool empty() const { return m_slots.empty(); }
    int size() const { return static_cast<int>(m_slots.size()); }
    int selectedIndex() const { return m_selected; }
    const Slot& at(int index) const { return m_slots[index]; }
    const Slot& selected() const { return m_slots[m_selected]; }
    float captureMultiplier() const { return empty() ? 1.0f : selected().type.captureMultiplier; }

    bool canThrow() const { return !empty() && selected().count > 0; }
    void consume() { if (canThrow()) --m_slots[m_selected].count; }

    // direction: +1 siguiente, -1 anterior (circular).
    void cycle(int direction) {
        const int n = size();
        if (n > 1) m_selected = ((m_selected + direction) % n + n) % n;
    }

    void store(std::map<std::string, int>& counts, std::string& selectedName) const {
        counts.clear();
        for (const Slot& slot : m_slots) counts[slot.type.name] = slot.count;
        selectedName = empty() ? std::string() : selected().type.name;
    }

    // Tipos que no aparecen en el guardado conservan sus unidades iniciales.
    void restore(const std::map<std::string, int>& counts, const std::string& selectedName) {
        for (int i = 0; i < size(); ++i) {
            Slot& slot = m_slots[i];
            if (const auto it = counts.find(slot.type.name); it != counts.end()) slot.count = it->second < 0 ? 0 : it->second;
            if (slot.type.name == selectedName) m_selected = i;
        }
    }

    private:
    std::vector<Slot> m_slots;
    int m_selected = 0;
};
