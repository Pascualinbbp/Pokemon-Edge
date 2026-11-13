#pragma once
#include <algorithm>
#include <cmath>

// Experiencia que da cada acción del jugador en el mundo (único sitio donde se define). Se irán añadiendo más.
namespace Xp {
    inline constexpr int GATHER = 6;    // terminar de recolectar un recurso (árbol, roca, planta...)
    inline constexpr int PICK_UP = 3;   // recoger un objeto suelto
    inline constexpr int CHEST = 12;    // abrir un cofre
    inline constexpr int CAPTURE = 25;  // capturar un pokémon
}

// Nivel, experiencia y medallas del jugador. El nivel máximo depende de las medallas de gimnasio; es también el
// límite de nivel de todos sus pokémon.
class PlayerProgress {
    public:
    static constexpr int START_LEVEL = 1;
    static constexpr int BASE_CAP = 15;       // nivel máximo sin medallas
    static constexpr int MAX_BADGES = 8;      // con todas las medallas el nivel máximo es MAX_LEVEL
    static constexpr int MAX_LEVEL = 100;
    static constexpr float XP_BASE = 40.0f;   // experiencia para subir de nivel: XP_BASE * nivel ^ XP_EXPONENT
    static constexpr float XP_EXPONENT = 1.4f;

    int level() const { return m_level; }
    int xp() const { return m_xp; }
    int badges() const { return m_badges; }
    unsigned badgeMask() const { return m_mask; }                  // qué medallas se tienen (una por cada líder; el bit es el id de la medalla)
    bool hasBadge(int id) const { return id >= 0 && id < MAX_BADGES && ((m_mask >> id) & 1u) != 0; }
    int levelCap() const { return BASE_CAP + (MAX_LEVEL - BASE_CAP) * m_badges / MAX_BADGES; } // reparte los niveles hasta 100 entre las medallas
    int xpToNext() const { return static_cast<int>(XP_BASE * std::pow(static_cast<float>(m_level), XP_EXPONENT)); }
    bool maxed() const { return m_level >= levelCap(); }
    float xpFraction() const { return maxed() ? 1.0f : static_cast<float>(m_xp) / static_cast<float>(xpToNext()); }

    // Suma experiencia. Devuelve cuántos niveles ha subido.
    int addXp(int amount) {
        int gained = 0;
        m_xp += (std::max)(amount, 0);
        while (!maxed() && m_xp >= xpToNext()) {
            m_xp -= xpToNext();
            ++m_level;
            ++gained;
        }
        if (maxed()) m_xp = 0;
        return gained;
    }

    // Da la medalla 'id' si no se tenía. Devuelve true si es nueva.
    bool addBadge(int id) {
        if (id < 0 || id >= MAX_BADGES || hasBadge(id)) return false;
        m_mask |= 1u << id;
        m_badges = count(m_mask);
        return true;
    }

    void restore(int level, int xp, unsigned mask) {
        m_mask = mask & ((1u << MAX_BADGES) - 1u);
        m_badges = count(m_mask);
        m_level = (std::clamp)(level, START_LEVEL, levelCap());
        m_xp = maxed() ? 0 : (std::clamp)(xp, 0, xpToNext() - 1);
    }

    private:
    static int count(unsigned mask) { int n = 0; for (; mask; mask >>= 1) n += static_cast<int>(mask & 1u); return n; }
    unsigned m_mask = 0;
    int m_level = START_LEVEL;
    int m_xp = 0;
    int m_badges = 0;
};
