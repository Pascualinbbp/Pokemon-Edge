#pragma once
#include "../../../models/gameData.hpp"

// Reglas de la tienda (aún sin tienda en el juego): regalos por compra. Se definen en la base de datos (tabla pokeball_gift).
namespace ShopRules {
    struct Gift { int ballId = -1; int count = 0; };

    // Compra de 'quantity' bolas del tipo 'ballId'. 'bought' acumula las compradas de ese tipo que aún no han dado regalo (se actualiza).
    // Devuelve la bola regalada y cuántas (por cada 'giftEvery' compradas, una).
    inline Gift giftFor(const GameData& data, int ballId, int quantity, int& bought) {
        const PokeballType* type = data.ball(ballId);
        if (!type || type->giftId < 0 || type->giftEvery <= 0 || quantity <= 0) return {};
        bought += quantity;
        const int count = bought / type->giftEvery;
        bought %= type->giftEvery;
        return { count > 0 ? type->giftId : -1, count };
    }
}
