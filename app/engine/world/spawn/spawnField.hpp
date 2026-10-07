#pragma once
#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

// Aparición en puntos fijos del mundo, común a todo lo que aparece así (cofres, nodos de recolección...).
// Un punto aloja una entidad a la vez; cuando esta termina ('finished()'), el punto espera 'delay' segundos
// y genera otra. La entidad debe tener spot() (el punto del que salió) y finished().
template <typename Entity>
class SpawnField {
    public:
    std::vector<Entity> entities;

    void setup(int spots) {
        entities.clear();
        m_timers.assign(spots, 0.0f); // 0 = aparece en cuanto empieza la partida
    }

    // step(Entity&): avanza una entidad (física, animación). make(spot) -> std::optional<Entity>: crea la del punto.
    template <typename Step, typename Make>
    void update(float dt, float delay, Step step, Make make) {
        for (Entity& entity : entities) {
            step(entity);
            if (entity.finished()) m_timers[entity.spot()] = delay;
        }
        entities.erase(std::remove_if(entities.begin(), entities.end(), [](const Entity& e) { return e.finished(); }), entities.end());

        for (int spot = 0; spot < static_cast<int>(m_timers.size()); ++spot) {
            if (occupied(spot)) continue;
            m_timers[spot] -= dt;
            if (m_timers[spot] > 0.0f) continue;

            std::optional<Entity> entity = make(spot);
            if (entity) entities.push_back(std::move(*entity));
            else m_timers[spot] = delay; // nada que generar: se reintenta más tarde
        }
    }

    private:
    bool occupied(int spot) const {
        for (const Entity& entity : entities) if (entity.spot() == spot) return true;
        return false;
    }

    std::vector<float> m_timers;
};
