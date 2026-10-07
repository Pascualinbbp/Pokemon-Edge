#pragma once
#include <optional>
#include <DirectXMath.h>
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"
#include "../entities/resourceNode.hpp"

// Dónde y cómo aparecen los nodos de recolección (pruebas): puntos fijos, como los pokémon.
// El tipo se elige al azar según spawn_weight.
namespace ResourceSpawn {
    inline constexpr float PLANT_MIN_GROWTH = 0.25f; // una planta nueva aparece con al menos este crecimiento
    inline constexpr float DELAY = 60.0f; // segundos desde que se agota un nodo hasta que aparece otro en su punto

    inline constexpr float SPOTS[][2] = {
        { -15.0f,   4.0f },
        {  16.0f,  -5.0f },
        {  -3.0f, -18.0f },
        {   3.0f,  22.0f },
        { -20.0f,  12.0f },
        {  20.0f,  14.0f },
        {   8.0f, -16.0f },
        { -14.0f, -18.0f },
        {  22.0f, -10.0f },
        {  -4.0f,  -6.0f },
    };
    inline constexpr int COUNT = static_cast<int>(sizeof(SPOTS) / sizeof(SPOTS[0]));

    inline std::optional<ResourceNode> make(const GameData& data, int spot) {
        const int type = RandomUtil::weightedIndex(data.nodes, [](const ResourceNodeType& node) { return node.spawnWeight; });
        if (type < 0) return std::nullopt;

        const ResourceNodeType& node = data.nodes[type];
        const float growth = node.plant() ? RandomUtil::range(PLANT_MIN_GROWTH, 1.0f) : 1.0f;
        return ResourceNode(spot, type, node.id, node.hits, static_cast<float>(node.growSeconds), growth,
                            DirectX::XMFLOAT3{ SPOTS[spot][0], 0.0f, SPOTS[spot][1] }, static_cast<float>(spot) * 1.7f);
    }
}
