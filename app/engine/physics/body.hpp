#pragma once
#include <DirectXMath.h>

namespace Physics {
    // Estado físico de cualquier entidad: personaje, pokémon, NPC, objeto móvil...
    struct Body {
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
        float gravityScale = 1.0f; // 0 = flota (objetos sin gravedad)
        float shadowRadius = 0.0f; // 0 = no proyecta sombra
        bool onGround = true;
    };
}