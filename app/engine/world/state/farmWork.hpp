#pragma once
#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"
#include "pokemonStorage.hpp"

// Pokémon que trabajan en la granja (hasta 6, los del menú de la granja). Cada uno trabaja según su habilidad (tabla pokemon.skill_id):
// Talar -> madera, Picar -> piedra y minerales, Regar -> riega las plantas (crecen antes) y recoge cultivos. Trabajan a ciclos y descansan entre ellos.
// En el mundo principal no existen: al llegar a la granja se calcula lo que han producido desde la última vez (con un tope) y se entrega.
// Dentro de la granja se ven trabajando y lo que producen va cayendo al inventario.
class FarmWork {
    public:
    static constexpr int   SKILL_CHOP = 1, SKILL_MINE = 2, SKILL_WATER = 3;
    static constexpr double MAX_OFFLINE = 8.0 * 3600.0;  // como mucho se acumulan 8 horas de trabajo
    static constexpr float CYCLE_SECONDS = 60.0f;        // un ciclo de trabajo con la habilidad a nivel 1 (cada nivel más lo acorta un 25 %)
    static constexpr float WORK_SECONDS = 45.0f;         // de cada 60 s trabajan 45 y descansan 15
    static constexpr float REST_SECONDS = 15.0f;
    static constexpr float LEVEL_SPEEDUP = 0.25f;
    static constexpr float WALK_SPEED = 3.5f;
    static constexpr float SIZE = 0.7f;

    // Materiales (tabla material): maderas de los árboles que se talan, piedra, mineral y cultivos.
    static constexpr int WOODS[] = { 1, 38, 39, 43 }; // roble, pino, abedul, acacia (los frutales no se talan)
    static constexpr int STONE = 2, IRON = 3, COAL = 4;
    static constexpr int CROPS[] = { 22, 23, 24, 30 }; // zanahoria, fresa, trigo, patata

    struct Worker {
        float x = 0.0f, z = 0.0f, yaw = 0.0f;
        float phase = 0.0f;   // segundo dentro del ciclo de trabajo + descanso
        float cycle = 0.0f;   // progreso hacia el siguiente producto (segundos de trabajo acumulados)
        bool working = false;
        bool placed = false;
        int plot = -1;        // parcela hacia la que trabaja
    };

    void restore(long long clock) { m_clock = clock; }
    // Valor que se guarda: dentro de la granja el reloj avanza con el jugador; fuera, queda en el momento en que se salió.
    long long clock(bool inFarm) const { return inFarm ? now() : m_clock; }
    void leave() { m_clock = now(); }
    const std::vector<Worker>& workers() const { return m_workers; }

    static long long now() { return static_cast<long long>(std::time(nullptr)); }

    // Habilidad con la que trabaja una especie (-1 = no sabe trabajar) y su nivel.
    static int skillOf(const PokemonSpecies& species) { return species.skillId == SKILL_CHOP || species.skillId == SKILL_MINE || species.skillId == SKILL_WATER ? species.skillId : -1; }
    static float cycleSeconds(const PokemonSpecies& species) { return CYCLE_SECONDS / (1.0f + LEVEL_SPEEDUP * static_cast<float>((std::max)(1, species.skillLevel) - 1)); }

    // Un producto del trabajo de esta especie: (material, unidades).
    static std::pair<int, int> product(const PokemonSpecies& species) {
        switch (skillOf(species)) {
            case SKILL_CHOP: return { WOODS[RandomUtil::integer(0, static_cast<int>(std::size(WOODS)) - 1)], RandomUtil::integer(1, 2) };
            case SKILL_MINE: {
                const int roll = RandomUtil::integer(0, 99);
                if (roll < 15 && species.skillLevel >= 2) return { IRON, 1 };
                if (roll < 35) return { COAL, 1 };
                return { STONE, RandomUtil::integer(1, 2) };
            }
            case SKILL_WATER: return { CROPS[RandomUtil::integer(0, static_cast<int>(std::size(CROPS)) - 1)], RandomUtil::integer(1, 2) };
            default: return { -1, 0 };
        }
    }

    // Trabajo acumulado mientras el jugador estaba fuera: devuelve lo producido por todos los trabajadores.
    std::map<int, int> settle(const GameData& data, const PokemonStorage& storage) {
        std::map<int, int> total;
        const long long t = now();
        double away = m_clock > 0 ? static_cast<double>(t - m_clock) : 0.0;
        m_clock = t;
        away = std::clamp(away, 0.0, MAX_OFFLINE);
        if (away < CYCLE_SECONDS) return total;
        for (const OwnedPokemon& owned : storage.farm()) {
            const PokemonSpecies* species = data.speciesById(owned.speciesId);
            if (!species || skillOf(*species) < 0) continue;
            const double cycles = away * (WORK_SECONDS / (WORK_SECONDS + REST_SECONDS)) / cycleSeconds(*species);
            int n = static_cast<int>(cycles);
            if (RandomUtil::roll(static_cast<float>((cycles - n) * 100.0))) ++n;
            for (int i = 0; i < n; ++i) {
                const std::pair<int, int> made = product(*species);
                if (made.first >= 0) total[made.first] += made.second;
            }
        }
        return total;
    }

    // Dentro de la granja: cada trabajador camina a su parcela, trabaja y descansa. 'plots' son las posiciones de las parcelas y 'home' el centro de la granja.
    // 'work(index, species)' se llama cuando un trabajador termina un ciclo (para regar y entregar el producto).
    template <class Spots, class Done>
    void update(float dt, const GameData& data, const PokemonStorage& storage, const Spots& plots, float homeX, float homeZ, Done work) {
        const int count = static_cast<int>(storage.farm().size());
        m_workers.resize(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            Worker& w = m_workers[static_cast<size_t>(i)];
            const PokemonSpecies* species = data.speciesById(storage.farm()[static_cast<size_t>(i)].speciesId);
            const float ring = 6.5f, angle = static_cast<float>(i) * 1.0471976f;
            const float restX = homeX + std::cos(angle) * ring, restZ = homeZ + std::sin(angle) * ring;
            if (!w.placed) { w.x = restX; w.z = restZ; w.placed = true; w.phase = static_cast<float>(i) * 7.0f; }
            const bool job = species && skillOf(*species) >= 0 && !plots.empty();
            w.phase = std::fmod(w.phase + dt, WORK_SECONDS + REST_SECONDS);
            const bool shift = job && w.phase < WORK_SECONDS;
            if (shift && !w.working) w.plot = (w.plot < 0 ? i * 3 : w.plot + 1 + i) % static_cast<int>(plots.size()); // empieza turno: va a otra parcela
            w.working = false;
            float tx = restX, tz = restZ;
            if (shift) {
                const auto& p = plots[static_cast<size_t>(w.plot)];
                const float dx = homeX - p.first, dz = homeZ - p.second, d = (std::max)(0.01f, std::sqrt(dx * dx + dz * dz));
                tx = p.first + dx / d * 1.3f;
                tz = p.second + dz / d * 1.3f;
            }
            const float dx = tx - w.x, dz = tz - w.z, d = std::sqrt(dx * dx + dz * dz);
            if (d > 0.15f) {
                const float step = (std::min)(d, WALK_SPEED * dt);
                w.x += dx / d * step;
                w.z += dz / d * step;
                w.yaw = std::atan2(dx, dz);
            } else if (shift) {
                const auto& p = plots[static_cast<size_t>(w.plot)];
                w.yaw = std::atan2(p.first - w.x, p.second - w.z); // mira a la parcela que trabaja
                w.working = true;
                w.cycle += dt;
                if (species && w.cycle >= cycleSeconds(*species)) {
                    w.cycle = 0.0f;
                    work(i, w.plot, *species);
                }
            }
        }
    }

    private:
    long long m_clock = 0;
    std::vector<Worker> m_workers;
};
