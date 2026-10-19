#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>
#include "playerProgress.hpp"
#include "../rules/evRules.hpp"
#include "../../../models/gameData.hpp"

// Progreso de cada especie en la Pokédex (capturas, variocolor, mejor rango, nivel más alto) y las recompensas ya reclamadas.
// Los logros y sus premios los define la tabla dex_reward; aquí solo se cuentan y se comparan.
class DexProgress {
    public:
    struct Entry {
        int captures = 0;
        int shiny = 0;
        int bestRank = -1;           // índice de EvRules::RANKS del mejor analizado (-1 = ninguno)
        int maxLevel = 0;
        std::vector<int> claimed;    // ids de dex_reward ya reclamados
    };

    void setData(const GameData& data) {
        m_data = &data;
        m_entries.assign(data.species.size(), {});
    }

    void captured(int speciesId, bool shiny, int level) {
        if (Entry* e = entry(speciesId)) {
            ++e->captures;
            if (shiny) ++e->shiny;
            e->maxLevel = (std::max)(e->maxLevel, level);
        }
    }
    void ranked(int speciesId, EvRules::Rank rank) { if (Entry* e = entry(speciesId)) e->bestRank = (std::max)(e->bestRank, static_cast<int>(rank)); }
    void leveled(int speciesId, int level) { if (Entry* e = entry(speciesId)) e->maxLevel = (std::max)(e->maxLevel, level); }

    const Entry* find(int speciesId) const {
        const int index = m_data ? m_data->speciesIndex(speciesId) : -1;
        return index >= 0 && index < static_cast<int>(m_entries.size()) ? &m_entries[index] : nullptr;
    }

    // Valor actual del logro de ese tipo para la especie.
    int value(int speciesId, DexReward::Kind kind) const {
        const Entry* e = find(speciesId);
        if (!e) return 0;
        switch (kind) {
            case DexReward::Kind::CAPTURES: return e->captures;
            case DexReward::Kind::SHINY:    return e->shiny;
            case DexReward::Kind::RANK:     return e->bestRank;
            case DexReward::Kind::LEVEL:    return e->maxLevel;
        }
        return 0;
    }
    bool reached(const DexReward& reward, int speciesId) const {
        return reward.appliesTo(speciesId) && value(speciesId, reward.kind) >= reward.threshold && (reward.kind != DexReward::Kind::RANK || value(speciesId, reward.kind) >= 0);
    }
    bool claimed(const DexReward& reward, int speciesId) const {
        const Entry* e = find(speciesId);
        return e && std::find(e->claimed.begin(), e->claimed.end(), reward.id) != e->claimed.end();
    }
    bool claimable(const DexReward& reward, int speciesId) const { return reached(reward, speciesId) && !claimed(reward, speciesId); }

    // Recompensas reclamables de una especie (o de todas con speciesId < 0).
    int claimableCount(int speciesId) const {
        int count = 0;
        const auto scan = [&](int id) { for (const DexReward& reward : m_data->dexRewards) if (claimable(reward, id)) ++count; };
        if (speciesId >= 0) scan(speciesId);
        else for (const PokemonSpecies& species : m_data->species) scan(species.id);
        return count;
    }

    // Puntos de progreso de la especie (los de sus hitos reclamados) y los que tendría con todos reclamados.
    int progress(int speciesId) const { return sumProgress(speciesId, true); }
    int progressMax(int speciesId) const { return sumProgress(speciesId, false); }

    struct Claim { bool done = false; int xp = 0; int levels = 0; };

    // Reclama el hito: da experiencia al jugador (multiplicada por la rareza de la especie) y suma su progreso. done = false si no se
    // puede (no alcanzado o ya reclamado).
    Claim claim(const DexReward& reward, int speciesId, PlayerProgress& player) {
        Entry* e = entry(speciesId);
        const PokemonSpecies* species = m_data ? m_data->speciesById(speciesId) : nullptr;
        if (!e || !species || !claimable(reward, speciesId)) return {};
        e->claimed.push_back(reward.id);
        const int xp = static_cast<int>(std::lround(static_cast<float>(reward.playerXp) * m_data->xpMultiplier(*species)));
        return { true, xp, player.addXp(xp) };
    }

    // Guardado: "Especie|capturas|variocolor|rango|nivel|id,id" (solo las especies con algo que contar).
    void store(std::vector<std::string>& out) const {
        out.clear();
        for (size_t i = 0; i < m_entries.size() && i < m_data->species.size(); ++i) {
            const Entry& e = m_entries[i];
            if (e.captures == 0 && e.bestRank < 0 && e.maxLevel == 0 && e.claimed.empty()) continue;
            std::string ids;
            for (const int id : e.claimed) ids += (ids.empty() ? "" : ",") + std::to_string(id);
            out.push_back(m_data->species[i].name + SEPARATOR + std::to_string(e.captures) + SEPARATOR + std::to_string(e.shiny) + SEPARATOR +
                          std::to_string(e.bestRank) + SEPARATOR + std::to_string(e.maxLevel) + SEPARATOR + ids);
        }
    }

    void restore(const std::vector<std::string>& lines) {
        m_entries.assign(m_data->species.size(), {});
        for (const std::string& line : lines) {
            std::vector<std::string> parts;
            std::stringstream stream(line);
            for (std::string part; std::getline(stream, part, SEPARATOR);) parts.push_back(part);
            if (line.back() == SEPARATOR) parts.push_back({});
            if (parts.size() < 6) continue;
            const int index = m_data->speciesIndexByName(parts[0]);
            if (index < 0) continue;
            Entry& e = m_entries[index];
            e.captures = std::atoi(parts[1].c_str());
            e.shiny = std::atoi(parts[2].c_str());
            e.bestRank = std::atoi(parts[3].c_str());
            e.maxLevel = std::atoi(parts[4].c_str());
            std::stringstream ids(parts[5]);
            for (std::string id; std::getline(ids, id, ',');) e.claimed.push_back(std::atoi(id.c_str()));
        }
    }

    private:
    static constexpr char SEPARATOR = '|';

    int sumProgress(int speciesId, bool claimedOnly) const {
        int total = 0;
        for (const DexReward& reward : m_data->dexRewards) {
            if (reward.appliesTo(speciesId) && (!claimedOnly || claimed(reward, speciesId))) total += reward.progress;
        }
        return total;
    }

    Entry* entry(int speciesId) {
        const int index = m_data ? m_data->speciesIndex(speciesId) : -1;
        return index >= 0 && index < static_cast<int>(m_entries.size()) ? &m_entries[index] : nullptr;
    }

    const GameData* m_data = nullptr;
    std::vector<Entry> m_entries;
};
