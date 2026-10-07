#pragma once
#include <optional>
#include <DirectXMath.h>
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"
#include "../entities/groundItem.hpp"

// Dónde aparecen los objetos sueltos (pruebas): puntos fijos, como los cofres. Qué dan se sortea al recogerlos
// según spawn_weight (tabla ground_item).
namespace GroundSpawn {
    inline constexpr float DELAY = 45.0f; // segundos desde que se recoge uno hasta que aparece otro en su punto

    inline constexpr float SPOTS[][2] = {
        {   2.0f,  -3.0f },
        {  -6.0f,   3.0f },
        {   8.0f,   5.0f },
        { -10.0f,  -2.0f },
        {   0.0f,  10.0f },
        {  13.0f,  -3.0f },
        {  -5.0f, -11.0f },
        {  17.0f,   9.0f },
    };
    inline constexpr int COUNT = static_cast<int>(sizeof(SPOTS) / sizeof(SPOTS[0]));

    inline std::optional<GroundItem> make(const GameData& data, int spot) {
        if (data.groundItems.empty()) return std::nullopt;
        return GroundItem(spot, DirectX::XMFLOAT3{ SPOTS[spot][0], 0.0f, SPOTS[spot][1] });
    }

    // Recompensa de recoger un objeto suelto: uno de la tabla ground_item según su peso. Devuelve nullptr si no hay.
    inline const GroundItemType* pickReward(const GameData& data) {
        return RandomUtil::pick(data.groundItems, [](const GroundItemType& g) { return g.spawnWeight; });
    }
}
