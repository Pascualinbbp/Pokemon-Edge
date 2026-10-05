#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "body.hpp"

namespace Physics {
    // Reglas físicas compartidas por todas las entidades del mundo.
    class World {
        public:
        static constexpr float GRAVITY = 20.0f;
        static constexpr float HALF_SIZE = 40.0f; // el mundo va de -HALF_SIZE a +HALF_SIZE en X y Z

        // Sólido: caja alineada con los ejes (paredes, hitbox de un pokémon...).
        struct Box {
            DirectX::XMFLOAT3 center;
            DirectX::XMFLOAT3 half; // semidimensiones
        };
        std::vector<Box> obstacles; // fijos (paredes)
        std::vector<Box> creatures; // hitbox de los pokémon: la escena los rehace cada frame
        std::vector<Box> props;     // objetos sólidos del mundo (cofres...): sólidos para todos los cuerpos; la escena los rehace cada frame

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
            collide(body);

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
        // Saca el cuerpo de los sólidos (en horizontal). Los proyectiles rebotan; el resto desliza.
        void collide(Body& body) const {
            if (body.collisionRadius <= 0.0f) return;
            const float low = body.position.y - body.groundOffset;
            const float high = low + body.collisionHeight;

            for (const Box& box : obstacles) pushOut(body, box, low, high);
            for (const Box& box : props) pushOut(body, box, low, high);
            if (body.hitsCreatures) for (const Box& box : creatures) pushOut(body, box, low, high);
        }

        static void pushOut(Body& body, const Box& box, float low, float high) {
            if (high <= box.center.y - box.half.y || low >= box.center.y + box.half.y) return;

            const float r = body.collisionRadius;
            const float minX = box.center.x - box.half.x, maxX = box.center.x + box.half.x;
            const float minZ = box.center.z - box.half.z, maxZ = box.center.z + box.half.z;
            const float px = body.position.x, pz = body.position.z;
            const float dx = px - std::clamp(px, minX, maxX);
            const float dz = pz - std::clamp(pz, minZ, maxZ);
            const float d2 = dx * dx + dz * dz;
            if (d2 >= r * r && d2 > 0.0f) return;

            float nx, nz;
            if (d2 > 0.0f) { // fuera de la caja, pero tocándola
                const float d = std::sqrt(d2);
                nx = dx / d;
                nz = dz / d;
                body.position.x += nx * (r - d);
                body.position.z += nz * (r - d);
            } else {         // el centro quedó dentro: se saca por la cara más cercana
                const float left = px - minX, right = maxX - px, back = pz - minZ, front = maxZ - pz;
                const float nearest = (std::min)((std::min)(left, right), (std::min)(back, front));
                nx = nz = 0.0f;
                if (nearest == left)       { nx = -1.0f; body.position.x = minX - r; }
                else if (nearest == right) { nx = 1.0f;  body.position.x = maxX + r; }
                else if (nearest == back)  { nz = -1.0f; body.position.z = minZ - r; }
                else                       { nz = 1.0f;  body.position.z = maxZ + r; }
            }

            const float vn = body.velocity.x * nx + body.velocity.z * nz;
            if (vn < 0.0f) {
                const float k = 1.0f + body.restitution; // sin restitución: se anula la velocidad contra el sólido
                body.velocity.x -= k * vn * nx;
                body.velocity.z -= k * vn * nz;
            }
        }

        static constexpr float BOUNCE_MIN_SPEED = 1.5f; // por debajo de esta velocidad de caída ya no bota
        static constexpr float BOUNCE_FRICTION = 0.8f;  // fracción de velocidad horizontal que se conserva en cada bote
    };
}
