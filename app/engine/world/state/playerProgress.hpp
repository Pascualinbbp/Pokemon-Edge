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
    static constexpr int CAP_PER_BADGE = 5;   // nivel máximo extra por cada medalla
    static constexpr int MAX_BADGES = 8;
    static constexpr float XP_BASE = 40.0f;   // experiencia para subir de nivel: XP_BASE * nivel ^ XP_EXPONENT
    static constexpr float XP_EXPONENT = 1.4f;

    int level() const { return m_level; }
    int xp() const { return m_xp; }
    int badges() const { return m_badges; }
    int levelCap() const { return BASE_CAP + CAP_PER_BADGE * m_badges; }
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

    void addBadge() { m_badges = (std::min)(m_badges + 1, MAX_BADGES); }

    void restore(int level, int xp, int badges) {
        m_badges = (std::clamp)(badges, 0, MAX_BADGES);
        m_level = (std::clamp)(level, START_LEVEL, levelCap());
        m_xp = maxed() ? 0 : (std::clamp)(xp, 0, xpToNext() - 1);
    }

    private:
    int m_level = START_LEVEL;
    int m_xp = 0;
    int m_badges = 0;
};
