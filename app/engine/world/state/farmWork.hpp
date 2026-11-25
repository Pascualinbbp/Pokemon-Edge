#pragma once
#include <algorithm>
#include <cmath>
#include <ctime>
#include <vector>
#include "../../../models/gameData.hpp"
#include "pokemonStorage.hpp"

// Pokémon que trabajan en el hogar (la base del jugador): hasta FARM_SLOTS, los del menú del hogar. Dos oficios, cada uno con su habilidad (tabla skill):
//  - Regar: va de planta en planta (también repitiendo) dándoles agua, así crecen antes.
//  - Recolectar: recoge lo que ya está listo (cultivos y frutos) y siembra de nuevo las parcelas designadas si hay semillas.
// Trabajan a ciclos y descansan entre ellos. Qué hacen y qué producen depende de lo que haya en el hogar (ver Scene::stepCrew).
// En el mundo principal no existen: al volver se simula el tiempo transcurrido (con tope MAX_OFFLINE). Dentro del hogar se ven trabajando.
class FarmWork {
    public:
    static constexpr int   SKILL_CHOP = 1, SKILL_MINE = 2, SKILL_WATER = 3, SKILL_GATHER = 5;
    static constexpr double MAX_OFFLINE = 2.0 * 3600.0;  // como mucho se simulan 2 horas de ausencia
    static constexpr float CYCLE_SECONDS = 12.0f;        // una tarea con la habilidad a nivel 1 (cada nivel más la acorta un 25 %)
    static constexpr float WORK_SECONDS = 45.0f;         // de cada 60 s trabajan 45 y descansan 15
    static constexpr float REST_SECONDS = 15.0f;
    static constexpr float LEVEL_SPEEDUP = 0.25f;
    static constexpr float WALK_SPEED = 3.5f;
    static constexpr float SIZE = 0.7f;
    static constexpr float STEP_SECONDS = 5.0f;          // paso de la simulación de la ausencia

    static_assert(CYCLE_SECONDS < WORK_SECONDS, "una tarea debe caber en el turno de trabajo (si no, nunca termina)");

    enum class Role { NONE, WATER, GATHER, CHOP, MINE };

    struct Worker {
        float x = 0.0f, z = 0.0f, yaw = 0.0f;
        float phase = 0.0f;   // segundo dentro del ciclo de trabajo + descanso
        float cycle = 0.0f;   // segundos trabajando en la tarea actual
        bool working = false;
        bool placed = false;
        int plot = -1;        // parcela de la tarea actual (-1 = ninguna)
        int last = -1;        // última parcela atendida (el riego va rotando)
    };

    static Role roleOf(const PokemonSpecies& species) {
        return species.skillId == SKILL_WATER ? Role::WATER : species.skillId == SKILL_GATHER ? Role::GATHER : species.skillId == SKILL_CHOP ? Role::CHOP : species.skillId == SKILL_MINE ? Role::MINE : Role::NONE;
    }
    static float speed(const PokemonSpecies& species) { return 1.0f + LEVEL_SPEEDUP * static_cast<float>((std::max)(1, species.skillLevel) - 1); }
    static float cycleSeconds(const PokemonSpecies& species) { return CYCLE_SECONDS / speed(species); }

    void restore(long long clock) { m_clock = clock; }
    // Valor que se guarda: dentro del hogar el reloj avanza con el jugador; fuera, queda en el momento en que se salió.
    long long clock(bool inHome) const { return inHome ? now() : m_clock; }
    void leave() { m_clock = now(); }
    static long long now() { return static_cast<long long>(std::time(nullptr)); }

    // Segundos de ausencia a simular (con tope); deja el reloj en el momento actual.
    double settle() {
        const long long t = now();
        const double away = m_clock > 0 ? static_cast<double>(t - m_clock) : 0.0;
        m_clock = t;
        return std::clamp(away, 0.0, MAX_OFFLINE);
    }

    std::vector<Worker>& crew() { return m_workers; }
    const std::vector<Worker>& workers() const { return m_workers; }
    void sync(size_t count) { m_workers.resize(count); }

    private:
    long long m_clock = 0;
    std::vector<Worker> m_workers;
};
