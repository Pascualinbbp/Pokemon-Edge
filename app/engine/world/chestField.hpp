#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "../../models/gameData.hpp"
#include "../../utils/core/randomUtil.hpp"
#include "chest.hpp"
#include "chestRules.hpp"

// Aparición de cofres para las pruebas, igual que los pokémon: puntos fijos del mundo. En cada punto hay un cofre
// (de un tipo elegido al azar según spawn_weight); al abrirlo, el punto tarda un rato en generar otro.
class ChestField {
    public:
    static constexpr float RESPAWN_DELAY = 20.0f; // segundos desde que se abre un cofre hasta que aparece otro en su punto

    std::vector<Chest> chests;

    void setup(const GameData& data) {
        m_data = &data;
        chests.clear();
        m_timers.assign(SPOT_COUNT, 0.0f); // 0 = aparece en cuanto empieza la partida
    }

    void update(float dt, const Physics::World& world) {
        if (!m_data) return;

        for (Chest& chest : chests) {
            world.step(chest.body, dt);
            chest.update(dt);
            if (chest.finished()) m_timers[chest.spot()] = RESPAWN_DELAY;
        }
        chests.erase(std::remove_if(chests.begin(), chests.end(), [](const Chest& chest) { return chest.finished(); }), chests.end());

        for (int spot = 0; spot < SPOT_COUNT; ++spot) {
            if (occupied(spot)) continue;
            m_timers[spot] -= dt;
            if (m_timers[spot] <= 0.0f) spawn(spot);
        }
    }

    private:
    // Posición (x, z) de cada punto de aparición. El jugador empieza en el origen mirando a +Z.
    static constexpr float SPOTS[][2] = {
        {   5.0f,  -6.0f },
        {  -3.0f,   8.0f },
        {  11.0f,   1.0f },
        { -12.0f,  -9.0f },
    };
    static constexpr int SPOT_COUNT = static_cast<int>(sizeof(SPOTS) / sizeof(SPOTS[0]));

    bool occupied(int spot) const {
        for (const Chest& chest : chests) if (chest.spot() == spot) return true;
        return false;
    }

    void spawn(int spot) {
        const int type = ChestRules::pickType(m_data->chests);
        if (type < 0) {
            m_timers[spot] = RESPAWN_DELAY; // sin tipos de cofre no hay nada que generar: se reintenta más tarde
            return;
        }

        const float x = SPOTS[spot][0], z = SPOTS[spot][1];
        chests.emplace_back(spot, type, m_data->chests[type].rarity, DirectX::XMFLOAT3{ x, 0.0f, z }, std::atan2(-x, -z)); // de cara al centro
    }

    const GameData* m_data = nullptr;
    std::vector<float> m_timers;
};
