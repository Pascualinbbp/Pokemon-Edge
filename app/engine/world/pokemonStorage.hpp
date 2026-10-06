#pragma once
#include <string>
#include <vector>
#include "../../models/gameData.hpp"

// Un pokémon del jugador: su especie y la habilidad activa que lleva equipada (entre las que puede aprender).
struct OwnedPokemon {
    int speciesId = -1;
    int abilityId = -1;
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
    bool add(int speciesId, bool& sentToPc) {
        const PokemonSpecies* species = m_data->speciesById(speciesId);
        if (!species) return false;
        const OwnedPokemon owned = { speciesId, species->defaultActiveAbility() };
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

    // Cambia la habilidad activa de un pokémon (solo entre las que puede aprender).
    void setAbility(bool inTeam, int index, int abilityId) {
        std::vector<OwnedPokemon>& list = inTeam ? m_team : m_pc;
        if (index < 0 || index >= static_cast<int>(list.size())) return;
        const PokemonSpecies* species = m_data->speciesById(list[index].speciesId);
        if (species && species->canLearn(abilityId)) list[index].abilityId = abilityId;
    }

    // --- Guardado ("Especie|Habilidad", por nombre) ---
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
            const Ability* ability = m_data->ability(owned.abilityId);
            result.push_back(species->name + SEPARATOR + (ability ? ability->name : std::string()));
        }
        return result;
    }

    void decode(const std::string& text, std::vector<OwnedPokemon>& list) const {
        const size_t cut = text.find(SEPARATOR);
        const int index = m_data->speciesIndexByName(text.substr(0, cut));
        if (index < 0) return;

        const PokemonSpecies& species = m_data->species[index];
        OwnedPokemon owned = { species.id, species.defaultActiveAbility() };
        if (cut != std::string::npos) {
            const std::string abilityName = text.substr(cut + 1);
            for (const Ability& ability : m_data->abilities) {
                if (ability.name == abilityName && species.canLearn(ability.id)) owned.abilityId = ability.id;
            }
        }
        list.push_back(owned);
    }

    const GameData* m_data = &GameData::empty();
    std::vector<OwnedPokemon> m_team;
    std::vector<OwnedPokemon> m_pc;
};
