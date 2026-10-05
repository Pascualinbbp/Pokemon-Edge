#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "../../models/gameData.hpp"
#include "../../utils/core/randomUtil.hpp"
#include "chest.hpp"
#include "chestRules.hpp"

// Aparición de cofres en las zonas de la base de datos: un cofre a la vez por zona y un máximo de apariciones
// por día del juego. Al abrirse un cofre, el siguiente tarda un tiempo aleatorio en aparecer.
class ChestField {
    public:
    static constexpr float RESPAWN_MIN = 20.0f; // segundos tras abrir un cofre hasta que la zona puede generar otro
    static constexpr float RESPAWN_MAX = 45.0f;

    std::vector<Chest> chests;

    void setup(const GameData& data) {
        m_data = &data;
        chests.clear();
        m_zones.assign(data.zones.size(), {});
    }

    void update(float dt, int day, const Physics::World& world) {
        if (!m_data) return;

        for (Chest& chest : chests) {
            world.step(chest.body, dt);
            chest.update(dt);
        }
        for (const Chest& chest : chests) if (chest.finished()) m_zones[chest.zone()].timer = RandomUtil::range(RESPAWN_MIN, RESPAWN_MAX);
        chests.erase(std::remove_if(chests.begin(), chests.end(), [](const Chest& chest) { return chest.finished(); }), chests.end());

        for (size_t i = 0; i < m_zones.size(); ++i) {
            ZoneState& state = m_zones[i];
            if (state.day != day) {
                state.day = day;
                state.spawned = 0;
            }
            if (occupied(static_cast<int>(i)) || state.spawned >= m_data->zones[i].maxPerDay) continue;

            state.timer -= dt;
            if (state.timer <= 0.0f) spawn(static_cast<int>(i));
        }
    }

    private:
    struct ZoneState {
        int day = -1;
        int spawned = 0;   // cofres que ha generado hoy
        float timer = 0.0f; // segundos hasta el próximo (0 = en cuanto se pueda)
    };

    bool occupied(int zone) const {
        for (const Chest& chest : chests) if (chest.zone() == zone) return true;
        return false;
    }

    void spawn(int zone) {
        const int type = ChestRules::pickType(m_data->chests);
        ZoneState& state = m_zones[zone];
        if (type < 0) {
            state.spawned = m_data->zones[zone].maxPerDay; // sin tipos de cofre no hay nada que generar hoy
            return;
        }

        const ChestZone& place = m_data->zones[zone];
        const float yaw = std::atan2(-place.x, -place.z); // de cara al centro del mundo
        chests.emplace_back(zone, type, m_data->chests[type].rarity, DirectX::XMFLOAT3{ place.x, 0.0f, place.z }, yaw);
        ++state.spawned;
    }

    const GameData* m_data = nullptr;
    std::vector<ZoneState> m_zones;
};
