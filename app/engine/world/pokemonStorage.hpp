#pragma once
#include <cstdlib>
#include <string>
#include <vector>
#include "../../models/gameData.hpp"
#include "evRules.hpp"

// Un pokémon del jugador: su especie y los EVs con los que se capturó.
struct OwnedPokemon {
    int speciesId = -1;
    EvRules::Evs evs = {};
};

// Pokémon del jugador: el equipo (hasta 6; el primero es el que lo acompaña por el mundo) y el PC donde se guardan
// los demás.
class PokemonStorage {
    public:
    static constexpr int TEAM_SIZE = 6;
    static constexpr int PC_CAPACITY = 120;

    void setData(const GameData& data) {
        m_data = &data;
        m_team.clear();
        m_pc.clear();
    }

    const std::vector<OwnedPokemon>& team() const { return m_team; }
    const std::vector<OwnedPokemon>& pc() const { return m_pc; }
    bool teamFull() const { return static_cast<int>(m_team.size()) >= TEAM_SIZE; }
    bool pcFull() const { return static_cast<int>(m_pc.size()) >= PC_CAPACITY; }
    int leadSpeciesId() const { return m_team.empty() ? -1 : m_team.front().speciesId; }

    // Un pokémon recién capturado va al equipo y, si está lleno, al PC. Devuelve false si no hay sitio en ninguno.
    bool add(const OwnedPokemon& owned, bool& sentToPc) {
        sentToPc = teamFull();
        if (!sentToPc) m_team.push_back(owned);
        else if (!pcFull()) m_pc.push_back(owned);
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

    // Libera (elimina) un pokémon del equipo o del PC.
    void release(bool inTeam, int index) {
        std::vector<OwnedPokemon>& list = inTeam ? m_team : m_pc;
        if (index >= 0 && index < static_cast<int>(list.size())) list.erase(list.begin() + index);
    }

    // --- Guardado ("Especie|ev,ev,ev,ev,ev,ev", por nombre de especie) ---
    void store(std::vector<std::string>& team, std::vector<std::string>& pc) const {
        team = encode(m_team);
        pc = encode(m_pc);
    }

    void restore(const std::vector<std::string>& team, const std::vector<std::string>& pc) {
        m_team.clear();
        m_pc.clear();
        for (const std::string& text : team) if (!teamFull()) decode(text, m_team);
        for (const std::string& text : pc) if (!pcFull()) decode(text, m_pc);
    }

    private:
    static constexpr char SEPARATOR = '|';

    std::vector<std::string> encode(const std::vector<OwnedPokemon>& list) const {
        std::vector<std::string> result;
        for (const OwnedPokemon& owned : list) {
            const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
            if (!species) continue;
            std::string text = species->name + SEPARATOR;
            for (int i = 0; i < BaseStats::COUNT; ++i) text += (i ? "," : "") + std::to_string(owned.evs[i]);
            result.push_back(std::move(text));
        }
        return result;
    }

    void decode(const std::string& text, std::vector<OwnedPokemon>& list) const {
        const size_t cut = text.find(SEPARATOR);
        const int index = m_data->speciesIndexByName(text.substr(0, cut));
        if (index < 0) return;

        OwnedPokemon owned;
        owned.speciesId = m_data->species[index].id;
        if (cut != std::string::npos) {
            size_t from = cut + 1;
            for (int i = 0; i < BaseStats::COUNT && from <= text.size(); ++i) {
                owned.evs[i] = std::atoi(text.c_str() + from);
                const size_t comma = text.find(',', from);
                if (comma == std::string::npos) break;
                from = comma + 1;
            }
            owned.evs = EvRules::clamped(owned.evs);
        }
        list.push_back(owned);
    }

    const GameData* m_data = &GameData::empty();
    std::vector<OwnedPokemon> m_team;
    std::vector<OwnedPokemon> m_pc;
};
