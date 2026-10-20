#pragma once
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/body.hpp"

// Aspecto de cada tipo de pokéball (por id de la base de datos). Es el único sitio donde se registra una pokéball nueva: su modelo
// (assets/models/<model>.glb, con los nodos "fixed" y "movable" y la articulación en extras.janus) y sus parámetros. Todo lo demás
// (lanzamiento, captura, cambio de pokémon, dibujo) funciona igual para cualquiera.
namespace PokeballStyle {
    inline constexpr const char* LID_PART = "movable";   // nodo de la tapa en cualquier modelo de pokéball
    inline constexpr const char* DEFAULT_MODEL = "pokeball/pokeball";

    struct Look {
        DirectX::XMFLOAT3 color;   // color del tipo (iconos y, si el modelo es teñible, sus zonas rojas)
        const char* model;         // modelo dentro de assets/models (sin .glb)
        bool tintable;             // true: las zonas rojas de la textura toman 'color'; false: el modelo trae su propia textura
        float frontYaw;            // giro (rad) para que el botón mire a +Z con yaw 0, si el modelo lo trae mirando a otro lado
    };

    inline const Look& look(int typeId) {
        static const Look pokeball  = { { 0.86f, 0.16f, 0.16f }, DEFAULT_MODEL, true, 0.0f };           // Poké Ball: roja
        static const Look superBall = { { 0.16f, 0.39f, 0.90f }, "pokeball/superball", false, 0.0f };  // Super Ball: modelo propio
        static const Look ultraBall = { { 0.96f, 0.80f, 0.16f }, "pokeball/ultraball", false, 0.0f };  // Ultra Ball: modelo propio
        static const Look starter   = { { 0.62f, 0.26f, 0.85f }, "pokeball/starterball", false, 0.0f }; // Starter Ball: modelo propio (solo la lleva el pokémon inicial)
        static const Look shinyBall = { { 0.95f, 0.78f, 0.95f }, "pokeball/shinyball", false, 0.0f };   // Shiny Ball: modelo propio
        static const Look honorBall = { { 0.92f, 0.92f, 0.96f }, "pokeball/honorball", false, 0.0f };   // Honor Ball: modelo propio (regalo de la tienda)
        static const Look unknown   = { { 0.70f, 0.70f, 0.70f }, DEFAULT_MODEL, true, 0.0f };
        switch (typeId) {
            case 1:  return pokeball;
            case 2:  return superBall;
            case 3:  return ultraBall;
            case 4:  return starter;
            case 5:  return shinyBall;
            case 6:  return honorBall;
            default: return unknown;
        }
    }
    inline const DirectX::XMFLOAT3& color(int typeId) { return look(typeId).color; }
}

// Pokéball lanzada: una esfera pequeña con física propia (sin botes: al tocar el suelo rueda, sigue las cuestas y se frena).
struct Pokeball {
    static constexpr float RADIUS      = 0.15f;
    static constexpr float LIFETIME    = 8.0f;  // segundos hasta desaparecer
    static constexpr float DRAG        = 1.6f;
    static constexpr float GRAVITY_SCALE = 0.5f; // cae más despacio que el resto: la trayectoria es más tensa
    static constexpr float TWIST_RATE  = 9.0f;   // rad/s sobre su eje al volar
    static constexpr float ALIGN       = 14.0f;  // rapidez con la que el botón sigue la trayectoria
    static constexpr float UNTWIST     = 6.0f;   // al tocar el suelo deja de girar sobre su eje
    static constexpr float CONTACT_MARGIN = 0.03f; // holgura para detectar el contacto con un pokémon tras la colisión

    Physics::Body body;
    float age = 0.0f;
    bool spent = false;                             // alcanzó a un pokémon: se gasta aunque falle la captura
    int typeId = -1;                                // tabla pokeball: el tipo de bola lanzada
    float captureMultiplier = 1.0f;                 // del tipo de bola lanzada
    float shinyMultiplier = 1.0f;                   // ídem frente a un shiny
    float multiplierFor(bool shiny) const { return shiny ? shinyMultiplier : captureMultiplier; }
    float yaw = 0.0f;                               // orientación del modelo: hacia donde avanza
    float spin = 0.0f;                              // giro sobre el eje lateral: inclinación al volar, rodar en el suelo (rad)
    float twist = 0.0f;                             // giro sobre el eje del botón (rad)

    Pokeball() {
        body.gravityScale = GRAVITY_SCALE;
        body.groundOffset = RADIUS; // 'position' es el centro de la esfera
        body.rolls = true;
        body.groundDrag = DRAG;
        body.collisionRadius = RADIUS;
        body.collisionHeight = RADIUS * 2.0f;
        body.hitsCreatures = false; // no rebota sobre los pokémon: los captura al tocarlos (hitBy)
        body.onGround = false;
    }

    bool expired() const { return age >= LIFETIME; }

    // Orienta el modelo según el movimiento. En el aire el botón mira hacia donde avanza (siguiendo la trayectoria) y la bola gira
    // despacio sobre ese eje; en el suelo rueda hacia delante tanto como recorre.
    void roll(float dt) {
        const float flat = std::hypot(body.velocity.x, body.velocity.z);
        if (flat > 0.3f) yaw = std::atan2(body.velocity.x, body.velocity.z);
        if (body.onGround) {
            spin += flat * dt / RADIUS;
            twist -= twist * (std::min)(1.0f, UNTWIST * dt);
        } else {
            const float pitch = -std::atan2(body.velocity.y, flat); // positivo = botón hacia abajo
            const float k = (std::min)(1.0f, ALIGN * dt);
            spin = pitch + DirectX::XMScalarModAngle(spin - pitch) * (1.0f - k);
            twist += TWIST_RATE * dt;
        }
    }
};
