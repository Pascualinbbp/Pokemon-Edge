#pragma once
#include <optional>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"
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

    // Qué dará un objeto suelto: índice en ground_item sorteado según su peso (se decide al aparecer para poder mostrarlo).
    inline int pickReward(const GameData& data) {
        const GroundItemType* reward = RandomUtil::pick(data.groundItems, [](const GroundItemType& g) { return g.spawnWeight; });
        return reward ? static_cast<int>(reward - data.groundItems.data()) : -1;
    }

    inline std::optional<GroundItem> make(const GameData& data, int spot) {
        if (data.groundItems.empty()) return std::nullopt;
        GroundItem item(spot, DirectX::XMFLOAT3{ SPOTS[spot][0], Physics::World::START_HEIGHT, SPOTS[spot][1] });
        item.setReward(pickReward(data));
        return item;
    }
}
