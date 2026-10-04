#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "body.hpp"

namespace Physics {
    // Reglas físicas compartidas por todas las entidades del mundo.
    class World {
        public:
        static constexpr float GRAVITY = 20.0f;
        static constexpr float HALF_SIZE = 40.0f; // el mundo va de -HALF_SIZE a +HALF_SIZE en X y Z

        // Obstáculo sólido: caja alineada con los ejes (paredes).
        struct Box {
            DirectX::XMFLOAT3 center;
            DirectX::XMFLOAT3 half; // semidimensiones
        };
        std::vector<Box> obstacles;

        // ¿Hay algún obstáculo en el camino desde 'origin' en la dirección 'dir' (unitaria)? Sirve para saber si la luz llega.
        bool blocked(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir) const {
            const float o[3] = { origin.x, origin.y, origin.z };
            const float d[3] = { dir.x, dir.y, dir.z };
            for (const Box& box : obstacles) {
                const float c[3] = { box.center.x, box.center.y, box.center.z };
                const float h[3] = { box.half.x, box.half.y, box.half.z };
                float tMin = 0.0f, tMax = 1.0e9f;
                bool hit = true;
                for (int i = 0; i < 3 && hit; ++i) {
                    if (std::fabs(d[i]) < 1.0e-6f) {
                        hit = o[i] >= c[i] - h[i] && o[i] <= c[i] + h[i];
                        continue;
                    }
                    float t0 = (c[i] - h[i] - o[i]) / d[i];
                    float t1 = (c[i] + h[i] - o[i]) / d[i];
                    if (t0 > t1) std::swap(t0, t1);
                    tMin = (std::max)(tMin, t0);
                    tMax = (std::min)(tMax, t1);
                    hit = tMin <= tMax;
                }
                if (hit) return true;
            }
            return false;
        }

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
        // Empuja el cuerpo fuera de los obstáculos (en horizontal). Los proyectiles rebotan; el resto desliza.
        void collide(Body& body) const {
            if (body.collisionRadius <= 0.0f) return;
            const float low = body.position.y - body.groundOffset;
            const float high = low + body.collisionHeight;
            const float r = body.collisionRadius;

            for (const Box& box : obstacles) {
                if (high <= box.center.y - box.half.y || low >= box.center.y + box.half.y) continue;

                const float minX = box.center.x - box.half.x, maxX = box.center.x + box.half.x;
                const float minZ = box.center.z - box.half.z, maxZ = box.center.z + box.half.z;
                const float px = body.position.x, pz = body.position.z;
                const float dx = px - std::clamp(px, minX, maxX);
                const float dz = pz - std::clamp(pz, minZ, maxZ);
                const float d2 = dx * dx + dz * dz;
                if (d2 >= r * r && d2 > 0.0f) continue;

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
                    const float k = 1.0f + body.restitution; // sin restitución: se anula la velocidad contra la pared
                    body.velocity.x -= k * vn * nx;
                    body.velocity.z -= k * vn * nz;
                }
            }
        }

        static constexpr float BOUNCE_MIN_SPEED = 1.5f; // por debajo de esta velocidad de caída ya no bota
        static constexpr float BOUNCE_FRICTION = 0.8f;  // fracción de velocidad horizontal que se conserva en cada bote
    };
}
