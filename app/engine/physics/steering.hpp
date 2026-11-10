#pragma once
#include <cmath>
#include <DirectXMath.h>
#include "physicsWorld.hpp"

// Esquivar obstáculos al ir hacia un punto (compañero, salvajes y pokémon de entrenadores): se mira por delante con tres rayos
// (centro y los dos costados, separados el radio del cuerpo, para no engancharse en las esquinas) y, si algo tapa el rumbo deseado,
// se prueba cada vez más a los lados, primero por el lado que ya se estaba usando para no vacilar. Así rodean rocas, árboles o muros
// en vez de quedarse parados contra ellos.
namespace Steering {
    inline constexpr float PROBE = 2.4f;   // a qué distancia por delante se mira

    // 'side' recuerda el lado (±1) por el que se rodea; 0 = camino libre. 'limit' acota la distancia que se mira (no hace falta mirar más allá del objetivo).
    inline float avoid(const Physics::World& world, const DirectX::XMFLOAT3& from, float radius, float heading, int& side, float limit = PROBE) {
        const float probe = (std::min)(PROBE, (std::max)(limit, 0.6f));
        const auto clear = [&](float angle) {
            const float s = std::sin(angle), c = std::cos(angle);
            for (const float lateral : { 0.0f, -radius, radius }) {
                const DirectX::XMFLOAT3 a = { from.x + c * lateral, from.y, from.z - s * lateral };
                const DirectX::XMFLOAT3 b = { a.x + s * probe, a.y, a.z + c * probe };
                if (world.blocked(a, b)) return false;
            }
            return true;
        };
        if (clear(heading)) {
            side = 0;
            return heading;
        }
        const int first = side != 0 ? side : 1;
        for (const float offset : { 0.4f, 0.8f, 1.2f, 1.6f, 2.0f, 2.5f }) {
            for (const int s : { first, -first }) {
                if (!clear(heading + static_cast<float>(s) * offset)) continue;
                side = s;
                return heading + static_cast<float>(s) * offset;
            }
        }
        side = first;
        return heading + static_cast<float>(first) * 1.57f; // todo tapado: se desliza a lo largo del obstáculo
    }
}
