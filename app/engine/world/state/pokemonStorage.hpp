#pragma once
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"
#include "../rules/companionRules.hpp"
#include "../rules/evRules.hpp"
#include "../rules/pokemonRules.hpp"
#include "../rules/researchRules.hpp"

// Un pokémon del jugador. Su potencial (rango, EVs máximos) está oculto hasta analizarlo en la máquina de investigación.
struct OwnedPokemon {
    int uid = 0;                    // identifica a este pokémon concreto durante la partida (no se guarda)
    int speciesId = -1;
    int level = 1;
    bool shiny = false;
    int ballId = -1;                // pokéball con la que se capturó (tabla pokeball)
    bool analyzed = false;          // ya pasó por la máquina de investigación
    EvRules::Evs evs = {};          // EVs actuales: empiezan en 0 y suben entrenándolo
    EvRules::Evs evCaps = {};       // EVs máximos con los que salió (su potencial)
    int xp = 0;                     // experiencia hacia el siguiente nivel (no se guarda)
    CompanionRules::Mode mode = CompanionRules::Mode::COLLECT; // qué hace cuando acompaña al jugador (no se guarda)
};

// Resultado de un análisis de la máquina de investigación.
struct AnalysisEntry {
    int speciesId = -1;
    int level = 1;
    bool shiny = false;
    EvRules::Rank rank = EvRules::Rank::D;
    int reward = 0;
    bool released = false; // liberado automáticamente por tener menos potencial del pedido
};

struct AnalysisReport {
    std::vector<AnalysisEntry> entries;
    int total = 0; // pokémonedas ganadas
};

// Pokémon del jugador: el equipo (hasta 6; el primero es el que lo acompaña por el mundo) y el PC donde se guardan
// los demás.
class PokemonStorage {
    public:
    static constexpr int TEAM_SIZE = 6;
    static constexpr int PC_CAPACITY = 120;

    enum class LevelResult { NONE, LEVELED, EVOLVED };

    void setData(const GameData& data) {
        m_data = &data;
        m_team.clear();
        m_pc.clear();
        m_autoMin = EvRules::Rank::D;
        m_active = 0;
    }

    const std::vector<OwnedPokemon>& team() const { return m_team; }
    const std::vector<OwnedPokemon>& pc() const { return m_pc; }
    bool teamFull() const { return static_cast<int>(m_team.size()) >= TEAM_SIZE; }
    bool pcFull() const { return static_cast<int>(m_pc.size()) >= PC_CAPACITY; }
    bool empty() const { return m_team.empty() && m_pc.empty(); }

    // El pokémon que acompaña al jugador: uno del equipo. Cambiarlo no altera el orden del equipo.
    const OwnedPokemon* active() const { return m_team.empty() ? nullptr : &m_team[m_active]; }
    int activeIndex() const { return m_active; }
    void setActive(int teamIndex) { if (teamIndex >= 0 && teamIndex < static_cast<int>(m_team.size())) m_active = teamIndex; }
    void cycleActive(int direction) { if (!m_team.empty()) m_active = ((m_active + direction) % static_cast<int>(m_team.size()) + static_cast<int>(m_team.size())) % static_cast<int>(m_team.size()); }

    // Un pokémon recién capturado va al equipo y, si está lleno, al PC. Devuelve false si no hay sitio en ninguno.
    bool add(const OwnedPokemon& owned, bool& sentToPc) {
        sentToPc = teamFull();
        OwnedPokemon stored = owned;
        stored.uid = m_nextUid++;
        if (!sentToPc) m_team.push_back(stored);
        else if (!pcFull()) m_pc.push_back(stored);
        else return false;
        return true;
    }

    // Un lugar donde puede estar un pokémon: una posición del equipo o del PC. Un índice igual al tamaño de la lista es
    // "al final" (casilla vacía).
    struct Place {
        bool inTeam = true;
        int index = 0;
        bool operator==(const Place& other) const { return inTeam == other.inTeam && index == other.index; }
    };

    // Mueve un pokémon a otro lugar: si el destino está ocupado intercambian sus sitios; si está vacío pasa al final de esa
    // lista. El equipo nunca se queda sin pokémon ni pasa de 6, ni el PC de su capacidad. El activo sigue siendo el mismo
    // pokémon cuando solo cambia de posición. Devuelve false si no se pudo y deja en 'landed' dónde ha quedado.
    bool move(const Place& from, const Place& to, Place& landed) {
        std::vector<OwnedPokemon>& source = from.inTeam ? m_team : m_pc;
        std::vector<OwnedPokemon>& target = to.inTeam ? m_team : m_pc;
        if (from.index < 0 || from.index >= static_cast<int>(source.size()) || to.index < 0 || from == to) return false;

        if (to.index < static_cast<int>(target.size())) { // intercambio
            std::swap(source[from.index], target[to.index]);
            if (from.inTeam && to.inTeam) {
                if (m_active == from.index) m_active = to.index;
                else if (m_active == to.index) m_active = from.index;
            }
            landed = to;
            return true;
        }
        if (&source == &target) { // al final de su misma lista
            std::rotate(source.begin() + from.index, source.begin() + from.index + 1, source.end());
            if (from.inTeam) m_active = m_active == from.index ? static_cast<int>(source.size()) - 1 : m_active > from.index ? m_active - 1 : m_active;
            landed = { from.inTeam, static_cast<int>(source.size()) - 1 };
            return true;
        }
        const bool full = to.inTeam ? teamFull() : pcFull();
        if (full || (from.inTeam && m_team.size() <= 1)) return false;
        target.push_back(source[from.index]);
        if (from.inTeam) eraseFromTeam(from.index);
        else source.erase(source.begin() + from.index);
        landed = { to.inTeam, static_cast<int>(target.size()) - 1 };
        return true;
    }

    // ¿Merece confirmación antes de liberarlo? Los variocolor y los de mucho potencial (A o más, ya analizados).
    bool valuable(const OwnedPokemon& owned) const {
        const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
        return owned.shiny || (owned.analyzed && species && EvRules::rank(species->stats, owned.evCaps) >= EvRules::Rank::A);
    }

    // --- Liberar ---
    // El pokémon inicial (el que lleva la pokéball exclusiva) nunca se libera; el equipo no puede quedarse vacío.
    bool bound(const OwnedPokemon& owned) const {
        const PokeballType* ball = m_data->ball(owned.ballId);
        return ball && !ball->obtainable;
    }

    bool canRelease(bool inTeam, int index) const {
        const std::vector<OwnedPokemon>& list = inTeam ? m_team : m_pc;
        return index >= 0 && index < static_cast<int>(list.size()) && !bound(list[index]) && !(inTeam && m_team.size() <= 1);
    }

    // Libera los pokémon indicados (índices del equipo y del PC). Devuelve cuántos se liberaron.
    int release(std::vector<int> teamIndices, std::vector<int> pcIndices) {
        int released = 0;
        const auto drop = [&](std::vector<OwnedPokemon>& list, std::vector<int>& indices, bool inTeam) {
            std::sort(indices.begin(), indices.end(), std::greater<int>());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
            for (const int index : indices) {
                if (!canRelease(inTeam, index)) continue;
                if (inTeam) eraseFromTeam(index);
                else list.erase(list.begin() + index);
                ++released;
            }
        };
        drop(m_team, teamIndices, true);
        drop(m_pc, pcIndices, false);
        return released;
    }

    // --- Entrenamiento ---
    // Sube un nivel (sin pasar de 'cap', el límite del jugador). Si alcanza el nivel de evolución, evoluciona.
    LevelResult levelUp(bool inTeam, int index, int cap) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || owned->level >= cap) return LevelResult::NONE;
        ++owned->level;
        const PokemonSpecies* species = m_data->speciesById(owned->speciesId);
        const Evolution* evolution = species ? species->levelEvolution(owned->level, RandomUtil::range(0.0f, 1.0f)) : nullptr;
        if (evolution && m_data->speciesById(evolution->toId)) {
            owned->speciesId = evolution->toId;
            return LevelResult::EVOLVED;
        }
        return LevelResult::LEVELED;
    }

    // Experiencia de combate: sube de nivel (y evoluciona) sin pasar de 'cap'. Devuelve niveles ganados y si evolucionó.
    struct XpResult { int levels = 0; bool evolved = false; };
    XpResult addXp(int teamIndex, int amount, int cap) {
        XpResult result;
        OwnedPokemon* owned = at(true, teamIndex);
        if (!owned) return result;
        owned->xp += (std::max)(amount, 0);
        while (owned->level < cap && owned->xp >= PokemonRules::xpToNext(owned->level)) {
            owned->xp -= PokemonRules::xpToNext(owned->level);
            const LevelResult step = levelUp(true, teamIndex, cap);
            if (step == LevelResult::NONE) break;
            ++result.levels;
            result.evolved = result.evolved || step == LevelResult::EVOLVED;
        }
        if (owned->level >= cap) owned->xp = 0;
        return result;
    }

    // Modo del pokémon (qué hace cuando acompaña al jugador).
    void cycleMode(bool inTeam, int index) {
        if (OwnedPokemon* owned = at(inTeam, index)) owned->mode = CompanionRules::next(owned->mode);
    }

    // Suma hasta 'amount' EVs en una estadística (sin pasar de su máximo). Devuelve false si no cambia nada.
    bool train(bool inTeam, int index, int stat, int amount) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || !owned->analyzed || stat < 0 || stat >= BaseStats::COUNT) return false;
        const int gain = (std::min)(amount, owned->evCaps[stat] - owned->evs[stat]);
        if (gain <= 0) return false;
        owned->evs[stat] += gain;
        return true;
    }

    // --- Máquina de investigación ---
    int pendingCount() const {
        return static_cast<int>(std::count_if(m_team.begin(), m_team.end(), [](const OwnedPokemon& p) { return !p.analyzed; }) +
                                std::count_if(m_pc.begin(), m_pc.end(), [](const OwnedPokemon& p) { return !p.analyzed; }));
    }

    // Potencial mínimo que se conserva: los analizados con menos se liberan solos (los variocolor y el inicial, nunca).
    EvRules::Rank autoRelease() const { return m_autoMin; }
    void setAutoRelease(EvRules::Rank rank) { m_autoMin = rank; }

    // Analiza todos los pokémon pendientes: revela sus datos, calcula la recompensa y libera los que no llegan al mínimo.
    AnalysisReport analyze() {
        AnalysisReport report;
        const auto scan = [&](std::vector<OwnedPokemon>& list) {
            for (size_t i = 0; i < list.size();) {
                OwnedPokemon& owned = list[i];
                const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
                if (owned.analyzed || !species) { ++i; continue; }

                owned.analyzed = true;
                AnalysisEntry entry;
                entry.speciesId = owned.speciesId;
                entry.level = owned.level;
                entry.shiny = owned.shiny;
                entry.rank = EvRules::rank(species->stats, owned.evCaps);
                entry.reward = ResearchRules::reward(*m_data, *species, owned.level, owned.shiny);
                const bool drop = !owned.shiny && !bound(owned) && entry.rank < m_autoMin && (&list != &m_team || m_team.size() > 1);
                entry.released = drop;
                report.total += entry.reward;
                report.entries.push_back(entry);
                if (drop && &list == &m_team) eraseFromTeam(static_cast<int>(i));
                else if (drop) list.erase(list.begin() + i);
                else ++i;
            }
        };
        scan(m_pc);
        scan(m_team);
        return report;
    }

    // --- Guardado ("Especie|nivel|variocolor|bola|analizado|evs|máximos", por nombre) ---
    void store(std::vector<std::string>& team, std::vector<std::string>& pc) const {
        team = encode(m_team);
        pc = encode(m_pc);
    }

    void restore(const std::vector<std::string>& team, const std::vector<std::string>& pc, int autoRank, int active) {
        m_team.clear();
        m_pc.clear();
        for (const std::string& text : team) if (!teamFull()) decode(text, m_team);
        for (const std::string& text : pc) if (!pcFull()) decode(text, m_pc);
        m_autoMin = EvRules::RANKS[(std::clamp)(autoRank, 0, static_cast<int>(std::size(EvRules::RANKS)) - 1)];
        m_active = m_team.empty() ? 0 : (std::clamp)(active, 0, static_cast<int>(m_team.size()) - 1);
    }

    int autoRankIndex() const { return static_cast<int>(m_autoMin); }

    private:
    static constexpr char SEPARATOR = '|';

    // Quita a un pokémon del equipo manteniendo como activo al mismo (o al que ocupe su lugar si era él).
    void eraseFromTeam(int index) {
        m_team.erase(m_team.begin() + index);
        if (index < m_active) --m_active;
        m_active = m_team.empty() ? 0 : (std::min)(m_active, static_cast<int>(m_team.size()) - 1);
    }

    OwnedPokemon* at(bool inTeam, int index) {
        std::vector<OwnedPokemon>& list = inTeam ? m_team : m_pc;
        return index >= 0 && index < static_cast<int>(list.size()) ? &list[index] : nullptr;
    }

    static std::string join(const EvRules::Evs& values) {
        std::string text;
        for (int i = 0; i < BaseStats::COUNT; ++i) text += (i ? "," : "") + std::to_string(values[i]);
        return text;
    }

    static EvRules::Evs split(const std::string& text) {
        EvRules::Evs values = {};
        size_t from = 0;
        for (int i = 0; i < BaseStats::COUNT && from <= text.size(); ++i) {
            values[i] = std::atoi(text.c_str() + from);
            const size_t comma = text.find(',', from);
            if (comma == std::string::npos) break;
            from = comma + 1;
        }
        return EvRules::clamped(values);
    }

    std::vector<std::string> encode(const std::vector<OwnedPokemon>& list) const {
        std::vector<std::string> result;
        for (const OwnedPokemon& owned : list) {
            const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
            const PokeballType* ball = m_data->ball(owned.ballId);
            if (!species) continue;
            result.push_back(species->name + SEPARATOR + std::to_string(owned.level) + SEPARATOR + (owned.shiny ? "1" : "0") + SEPARATOR +
                             (ball ? ball->name : std::string()) + SEPARATOR + (owned.analyzed ? "1" : "0") + SEPARATOR +
                             join(owned.evs) + SEPARATOR + join(owned.evCaps));
        }
        return result;
    }

    void decode(const std::string& text, std::vector<OwnedPokemon>& list) const {
        std::vector<std::string> fields;
        for (size_t from = 0;;) {
            const size_t cut = text.find(SEPARATOR, from);
            fields.push_back(text.substr(from, cut == std::string::npos ? std::string::npos : cut - from));
            if (cut == std::string::npos) break;
            from = cut + 1;
        }
        const int index = m_data->speciesIndexByName(fields[0]);
        if (index < 0) return;

        const PokemonSpecies& species = m_data->species[index];
        const auto field = [&](size_t i) -> const std::string& { static const std::string none; return i < fields.size() ? fields[i] : none; };
        OwnedPokemon owned;
        owned.uid = m_nextUid++;
        owned.speciesId = species.id;
        owned.level = (std::max)(1, field(1).empty() ? species.minLevel : std::atoi(field(1).c_str()));
        owned.shiny = field(2) == "1";
        if (const PokeballType* ball = m_data->ballByName(field(3))) owned.ballId = ball->id;
        owned.analyzed = field(4) == "1";
        owned.evs = split(field(5));
        owned.evCaps = field(6).empty() ? EvRules::Evs{} : split(field(6));
        for (int i = 0; i < BaseStats::COUNT; ++i) owned.evs[i] = (std::min)(owned.evs[i], owned.evCaps[i]);
        list.push_back(owned);
    }

    const GameData* m_data = &GameData::empty();
    std::vector<OwnedPokemon> m_team;
    std::vector<OwnedPokemon> m_pc;
    EvRules::Rank m_autoMin = EvRules::Rank::D;
    int m_active = 0;  // índice en el equipo del pokémon que acompaña al jugador
    mutable int m_nextUid = 1;
};
