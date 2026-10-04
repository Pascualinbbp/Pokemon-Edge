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
        // Los cuerpos con restitution > 0 (proyectiles) botan y ruedan; el resto se quedan en el suelo.
        void step(Body& body, float dt) const {
            if (!body.onGround) body.velocity.y -= GRAVITY * body.gravityScale * dt;

            body.position.x = (std::clamp)(body.position.x + body.velocity.x * dt, -HALF_SIZE, HALF_SIZE);
            body.position.z = (std::clamp)(body.position.z + body.velocity.z * dt, -HALF_SIZE, HALF_SIZE);
            body.position.y += body.velocity.y * dt;

            const float floorY = groundHeight(body.position.x, body.position.z) + body.groundOffset;
            const bool touching = body.position.y <= floorY && body.velocity.y <= 0.0f;
            body.onGround = touching;
            if (!touching) return;

            body.position.y = floorY;

            // Bote: se pierde parte de la velocidad vertical y algo de la horizontal.
            if (body.restitution > 0.0f && -body.velocity.y > BOUNCE_MIN_SPEED) {
                body.velocity.y = -body.velocity.y * body.restitution;
                body.velocity.x *= BOUNCE_FRICTION;
                body.velocity.z *= BOUNCE_FRICTION;
                body.onGround = false;
                return;
            }

            body.velocity.y = 0.0f;
            if (body.groundDrag > 0.0f) {
                const float keep = (std::max)(0.0f, 1.0f - body.groundDrag * dt);
                body.velocity.x *= keep;
                body.velocity.z *= keep;
            }
        }

        private:
        static constexpr float BOUNCE_MIN_SPEED = 1.5f; // por debajo de esta velocidad de caída ya no bota
        static constexpr float BOUNCE_FRICTION = 0.8f;  // fracción de velocidad horizontal que se conserva en cada bote
    };
}
