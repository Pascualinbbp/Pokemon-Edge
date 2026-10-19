#pragma once
#include <vector>

// Tabla de tipos (tabla type_matchup): lo que multiplica el daño de un tipo atacante sobre un tipo defensor. Lo que no está en la
// tabla es x1. Es la única definición de efectividad; el cálculo de daño la consulta.
class TypeChart {
    public:
    static constexpr float IMMUNE = 0.0f;

    void add(int attacker, int defender, float multiplier) { m_entries.push_back({ attacker, defender, multiplier }); }

    float multiplier(int attacker, int defender) const {
        for (const Entry& entry : m_entries) if (entry.attacker == attacker && entry.defender == defender) return entry.multiplier;
        return 1.0f;
    }

    // Contra un defensor de uno o dos tipos se multiplican los factores de cada uno.
    float multiplier(int attacker, const std::vector<int>& defenders) const {
        float total = 1.0f;
        for (const int defender : defenders) total *= multiplier(attacker, defender);
        return total;
    }

    bool empty() const { return m_entries.empty(); }

    private:
    struct Entry { int attacker; int defender; float multiplier; };
    std::vector<Entry> m_entries;
};
