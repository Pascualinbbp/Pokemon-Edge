#pragma once
#include <string>
#include <vector>
#include "../../models/gameData.hpp"

// Pokémon del jugador: el equipo (hasta 6; el primero es el que lo acompaña por el mundo) y el PC donde se guardan
// los demás. Cada pokémon se identifica por el id de su especie.
class PokemonStorage {
    public:
    static constexpr int TEAM_SIZE = 6;
    static constexpr int PC_CAPACITY = 120;

    void setData(const GameData& data) {
        m_data = &data;
        m_team.clear();
        m_pc.clear();
    }

    const std::vector<int>& team() const { return m_team; }
    const std::vector<int>& pc() const { return m_pc; }
    bool teamFull() const { return static_cast<int>(m_team.size()) >= TEAM_SIZE; }
    bool pcFull() const { return static_cast<int>(m_pc.size()) >= PC_CAPACITY; }
    int leadSpeciesId() const { return m_team.empty() ? -1 : m_team.front(); }

    // Un pokémon recién capturado va al equipo y, si está lleno, al PC. Devuelve false si no hay sitio en ninguno.
    bool add(int speciesId, bool& sentToPc) {
        sentToPc = teamFull();
        if (!sentToPc) m_team.push_back(speciesId);
        else if (!pcFull()) m_pc.push_back(speciesId);
        else return false;
        return true;
    }

    void makeLead(int teamIndex) {
        if (teamIndex > 0 && teamIndex < static_cast<int>(m_team.size())) std::swap(m_team[0], m_team[teamIndex]);
    }

    void sendToPc(int teamIndex) {
        if (teamIndex < 0 || teamIndex >= static_cast<int>(m_team.size()) || pcFull()) return;
        m_pc.push_back(m_team[teamIndex]);
        m_team.erase(m_team.begin() + teamIndex);
    }

    void sendToTeam(int pcIndex) {
        if (pcIndex < 0 || pcIndex >= static_cast<int>(m_pc.size()) || teamFull()) return;
        m_team.push_back(m_pc[pcIndex]);
        m_pc.erase(m_pc.begin() + pcIndex);
    }

    // --- Guardado (por nombre de especie) ---
    void store(std::vector<std::string>& team, std::vector<std::string>& pc) const {
        team = names(m_team);
        pc = names(m_pc);
    }

    void restore(const std::vector<std::string>& team, const std::vector<std::string>& pc) {
        m_team.clear();
        m_pc.clear();
        for (const std::string& name : team) if (!teamFull()) addByName(name, m_team);
        for (const std::string& name : pc) if (!pcFull()) addByName(name, m_pc);
    }

    private:
    std::vector<std::string> names(const std::vector<int>& ids) const {
        std::vector<std::string> result;
        for (const int id : ids) if (const PokemonSpecies* species = m_data->speciesById(id)) result.push_back(species->name);
        return result;
    }

    void addByName(const std::string& name, std::vector<int>& list) const {
        if (const int index = m_data->speciesIndexByName(name); index >= 0) list.push_back(m_data->species[index].id);
    }

    const GameData* m_data = &GameData::empty();
    std::vector<int> m_team;
    std::vector<int> m_pc;
};
