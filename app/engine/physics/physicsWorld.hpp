#pragma once
#include <algorithm>
#include "body.hpp"

namespace Physics {
    // Reglas físicas compartidas por todas las entidades del mundo.
    class World {
        public:
        static constexpr float GRAVITY = 20.0f;
        static constexpr float HALF_SIZE = 40.0f; // el mundo va de -HALF_SIZE a +HALF_SIZE en X y Z

        // Altura del terreno en (x, z). De momento el suelo es plano; aquí irá el terreno generado.
        float groundHeight(float, float) const { return 0.0f; }

        void jump(Body& body, float speed) const {
            body.velocity.y = speed;
            body.onGround = false;
        }

        // Integra un cuerpo: gravedad, desplazamiento por su velocidad, límites del mundo y colisión con el suelo.
        // Quien controla la entidad (jugador, IA...) solo asigna body.velocity.x/z antes de llamar.
        void step(Body& body, float dt) const {
            if (!body.onGround) body.velocity.y -= GRAVITY * body.gravityScale * dt;

            body.position.x = (std::clamp)(body.position.x + body.velocity.x * dt, -HALF_SIZE, HALF_SIZE);
            body.position.z = (std::clamp)(body.position.z + body.velocity.z * dt, -HALF_SIZE, HALF_SIZE);
            body.position.y += body.velocity.y * dt;

            const float ground = groundHeight(body.position.x, body.position.z);
            body.onGround = body.position.y <= ground && body.velocity.y <= 0.0f;
            if (body.onGround) {
                body.position.y = ground;
                body.velocity.y = 0.0f;
            }
        }
    };
}