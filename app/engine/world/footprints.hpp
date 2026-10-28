#pragma once
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

// Huellas en la arena. Un búfer circular de tamaño fijo (las más viejas se pisan solas): cada huella nace al completar una zancada, dura
// un rato y se va borrando. Sirve para cualquier cosa que se mueva por el suelo: jugador, acompañante, salvajes, objetos que ruedan.
// La forma depende de quien pisa (botas, zarpa, ave, pezuña, redonda).
class Footprints {
    public:
    enum class Shape { BOOT, PAW, BIRD, HOOF, ROUND };
    static constexpr int   MAX  = 220;
    static constexpr float LIFE = 40.0f;   // segundos hasta borrarse del todo
    static constexpr float FADE = 0.35f;   // fracción final de la vida en que se desvanece

    struct Print { float x, y, z, yaw, size, age; Shape shape; bool left; };

    // La forma de un pokémon sale de su especie (siempre la misma para la misma especie).
    static Shape shapeOf(int speciesId) {
        switch (((speciesId % 4) + 4) % 4) {
            case 0: return Shape::PAW;
            case 1: return Shape::BIRD;
            case 2: return Shape::HOOF;
            default: return Shape::PAW;
        }
    }

    // Avanza a quien se mueve: si completa una zancada y 'canPrint(x, z)' (arena, fuera del agua...) deja una huella en 'y'.
    // 'key' identifica a quien camina (su dirección); 'size' es su tamaño aproximado en metros.
    template <typename CanPrint>
    void step(const void* key, float x, float y, float z, float yaw, float size, Shape shape, bool grounded, CanPrint canPrint) {
        Walker& walker = m_walkers[key];
        walker.seen = m_stamp;
        if (!walker.started) { walker.started = true; walker.x = x; walker.z = z; return; }
        const float d = std::hypot(x - walker.x, z - walker.z);
        walker.x = x;
        walker.z = z;
        if (!grounded) { walker.carry = 0.0f; return; }
        walker.carry += d;
        const float stride = 0.35f + size * 0.55f;
        if (walker.carry < stride) return;
        walker.carry -= stride;
        if (!canPrint(x, z)) return;
        walker.left = !walker.left;
        const float side = (walker.left ? -1.0f : 1.0f) * size * 0.18f;
        push({ x + std::cos(yaw) * side, y, z - std::sin(yaw) * side, yaw, size, 0.0f, shape, walker.left });
    }

    // Cada fotograma: envejece las huellas y olvida a quien ya no se ha visto (descargado, desaparecido).
    void update(float dt) {
        for (Print& p : m_prints) p.age += dt;
        for (auto it = m_walkers.begin(); it != m_walkers.end();) it = it->second.seen != m_stamp ? m_walkers.erase(it) : std::next(it);
        ++m_stamp;
    }

    const std::vector<Print>& prints() const { return m_prints; }
    static bool alive(const Print& p) { return p.age < LIFE; }
    // 1 = recién hecha; baja a 0 al borrarse.
    static float strength(const Print& p) {
        const float t = p.age / LIFE;
        return t < 1.0f - FADE ? 1.0f : (std::max)(0.0f, (1.0f - t) / FADE);
    }

    private:
    struct Walker { float x = 0.0f, z = 0.0f, carry = 0.0f; bool left = false, started = false; unsigned seen = 0; };

    void push(const Print& print) {
        if (static_cast<int>(m_prints.size()) < MAX) m_prints.push_back(print);
        else { m_prints[m_next] = print; m_next = (m_next + 1) % MAX; }
    }

    std::vector<Print> m_prints;
    std::unordered_map<const void*, Walker> m_walkers;
    int m_next = 0;
    unsigned m_stamp = 1;
};
