#pragma once
#include "../../../models/gameData.hpp"
#include "rewardRules.hpp"

// Reglas de la tienda (aún sin tienda en el juego): regalos por compra. Se definen en la base de datos (item.gift_every y item.gift_reward_id).
namespace ShopRules {
    struct Gift { const RewardEntry* reward = nullptr; int count = 0; };

    // Compra de 'quantity' unidades de un objeto. 'bought' acumula las compradas de ese objeto que aún no han dado regalo (se actualiza).
    // Devuelve la recompensa regalada y cuántas veces (una por cada 'giftEvery' compradas).
    inline Gift giftFor(const GameData& data, const Item& item, int quantity, int& bought) {
        if (item.giftRewardId < 0 || item.giftEvery <= 0 || quantity <= 0) return {};
        bought += quantity;
        const int count = bought / item.giftEvery;
        bought %= item.giftEvery;
        return { count > 0 ? RewardRules::pick(data, item.giftRewardId) : nullptr, count };
    }
}
