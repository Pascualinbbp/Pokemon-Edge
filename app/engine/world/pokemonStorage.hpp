#pragma once
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../models/gameData.hpp"
#include "evRules.hpp"
#include "researchRules.hpp"

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

    // Cambia de posición un pokémon del equipo con el anterior (-1) o el siguiente (+1); el activo sigue siendo el mismo.
    void moveInTeam(int teamIndex, int direction) {
        const int other = teamIndex + direction;
        if (teamIndex < 0 || other < 0 || teamIndex >= static_cast<int>(m_team.size()) || other >= static_cast<int>(m_team.size())) return;
        std::swap(m_team[teamIndex], m_team[other]);
        if (m_active == teamIndex) m_active = other;
        else if (m_active == other) m_active = teamIndex;
    }

    void sendToPc(int teamIndex) {
        if (teamIndex < 0 || teamIndex >= static_cast<int>(m_team.size()) || pcFull()) return;
        m_pc.push_back(m_team[teamIndex]);
        eraseFromTeam(teamIndex);
    }

    void sendToTeam(int pcIndex) {
        if (pcIndex < 0 || pcIndex >= static_cast<int>(m_pc.size()) || teamFull()) return;
        m_team.push_back(m_pc[pcIndex]);
        m_pc.erase(m_pc.begin() + pcIndex);
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
        if (species && species->evolvesToId > 0 && owned->level >= species->evolveLevel && m_data->speciesById(species->evolvesToId)) {
            owned->speciesId = species->evolvesToId;
            return LevelResult::EVOLVED;
        }
        return LevelResult::LEVELED;
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
