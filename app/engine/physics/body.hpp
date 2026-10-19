#pragma once
#include <DirectXMath.h>

namespace Physics {
    // Estado físico de cualquier entidad: personaje, pokémon, NPC, objeto móvil, proyectil...
    struct Body {
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
        float gravityScale = 1.0f; // 0 = flota (objetos sin gravedad)
        float groundOffset = 0.0f; // distancia de 'position' a la parte baja del cuerpo (0 = position son los pies; el radio si position es el centro)
        float restitution = 0.0f;  // 0 = no rebota; 0.5 = conserva la mitad de la velocidad vertical al botar
        float groundDrag = 0.0f;   // frenado horizontal por segundo mientras rueda por el suelo (0 = ninguno)
        float collisionRadius = 0.0f; // radio horizontal contra obstáculos (0 = los atraviesa)
        float collisionHeight = 0.0f; // altura del cuerpo desde la parte baja
        bool hitsCreatures = false;   // también choca con los pokémon sólidos (el jugador sí; las pokéballs los capturan)
        bool onGround = true;
        bool stickToGround = false; // al bajar una cuesta se queda pegado al suelo en vez de caer (personajes)
        bool rolls = false;         // rueda: en una cuesta gana velocidad cuesta abajo (pokéballs)
        float feet() const { return position.y - groundOffset; } // altura de la parte baja del cuerpo
        bool underground = false;   // dentro de una galería (cueva o mina): la física la fija en cada paso y las consultas usan el suelo de la galería
        bool underwater = false;    // buceando: sin gravedad ni flotación; quien lo controla fija la velocidad vertical
    };
}
