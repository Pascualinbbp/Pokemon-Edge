#pragma once
#include <cmath>
#include <optional>
#include <DirectXMath.h>
#include "../../../models/gameData.hpp"
#include "../entities/chest.hpp"
#include "../rules/chestRules.hpp"

// Dónde y cómo aparecen los cofres (pruebas): puntos fijos, como los pokémon. El tipo se elige al azar según spawn_weight.
namespace ChestSpawn {
    inline constexpr float DELAY = 20.0f; // segundos desde que se abre un cofre hasta que aparece otro en su punto

    // Posición (x, z) de cada punto. El jugador empieza en el origen mirando a +Z.
    inline constexpr float SPOTS[][2] = {
        {   5.0f,  -6.0f },
        {  -3.0f,   8.0f },
        {  11.0f,   1.0f },
        { -12.0f,  -9.0f },
    };
    inline constexpr int COUNT = static_cast<int>(sizeof(SPOTS) / sizeof(SPOTS[0]));

    inline std::optional<Chest> make(const GameData& data, int spot) {
        const int type = ChestRules::pickType(data.chests);
        if (type < 0) return std::nullopt;

        const float x = SPOTS[spot][0], z = SPOTS[spot][1];
        return Chest(spot, type, data.chests[type].rarity, DirectX::XMFLOAT3{ x, 0.0f, z }, std::atan2(-x, -z)); // de cara al centro
    }
}
