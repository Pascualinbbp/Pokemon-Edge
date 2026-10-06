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
        static constexpr float STEP_HEIGHT = 0.3f; // desnivel que se sube andando; por debajo de la cara superior menos esto, el sólido bloquea
        static constexpr float SLIDE_SPEED = 5.0f; // velocidad con la que se resbala de lo que no admite quedarse encima

        // Sólido: caja alineada con los ejes (paredes, cofres, rocas, hitbox de un pokémon...).
        // walkable = se puede quedar encima; si no (pokémon, personajes), se puede saltar por encima pero se resbala al lado.
        struct Box {
            DirectX::XMFLOAT3 center;
            DirectX::XMFLOAT3 half; // semidimensiones
            bool walkable = true;

            float top() const { return center.y + half.y; }
            float bottom() const { return center.y - half.y; }
        };
        std::vector<Box> obstacles; // fijos (paredes)
        std::vector<Box> creatures; // hitbox de los pokémon: la escena los rehace cada frame
        std::vector<Box> props;     // objetos sólidos del mundo (cofres, rocas, árboles...): la escena los rehace cada frame

        // Altura del terreno en (x, z). De momento el suelo es plano; aquí irá el terreno generado.
        float groundHeight(float, float) const { return 0.0f; }

        // ¿Hay algún sólido fijo (paredes, rocas, árboles, cofres...) entre dos puntos? Los pokémon no cuentan.
        bool blocked(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to) const {
            const auto crosses = [&](const Box& box) {
                const float lo[3] = { box.center.x - box.half.x, box.center.y - box.half.y, box.center.z - box.half.z };
                const float hi[3] = { box.center.x + box.half.x, box.center.y + box.half.y, box.center.z + box.half.z };
                const float o[3] = { from.x, from.y, from.z };
                const float d[3] = { to.x - from.x, to.y - from.y, to.z - from.z };
                float tMin = 0.0f, tMax = 1.0f;
                for (int i = 0; i < 3; ++i) {
                    if (std::fabs(d[i]) < 1.0e-6f) {
                        if (o[i] < lo[i] || o[i] > hi[i]) return false;
                        continue;
                    }
                    float t0 = (lo[i] - o[i]) / d[i], t1 = (hi[i] - o[i]) / d[i];
                    if (t0 > t1) std::swap(t0, t1);
                    tMin = (std::max)(tMin, t0);
                    tMax = (std::min)(tMax, t1);
                    if (tMin > tMax) return false;
                }
                return true;
            };
            for (const Box& box : obstacles) if (crosses(box)) return true;
            for (const Box& box : props) if (crosses(box)) return true;
            return false;
        }

        void jump(Body& body, float speed) const {
            body.velocity.y = speed;
            body.onGround = false;
        }

        // Integra un cuerpo: gravedad, desplazamiento por su velocidad, límites del mundo, choque lateral con los
        // sólidos y suelo (el terreno o la cara superior de un sólido sobre el que está o cae).
        // Quien controla la entidad (jugador, IA...) solo asigna body.velocity.x/z antes de llamar.
        // Los cuerpos con restitution > 0 (proyectiles) botan y ruedan; el resto se quedan en el suelo.
        void step(Body& body, float dt) const {
            const float previousLow = body.position.y - body.groundOffset; // altura de los pies al empezar el paso
            if (!body.onGround) body.velocity.y -= GRAVITY * body.gravityScale * dt;

            body.position.x = (std::clamp)(body.position.x + body.velocity.x * dt, -HALF_SIZE, HALF_SIZE);
            body.position.z = (std::clamp)(body.position.z + body.velocity.z * dt, -HALF_SIZE, HALF_SIZE);
            body.position.y += body.velocity.y * dt;
            if (body.collisionRadius > 0.0f) pushOutAll(body, previousLow);

            const float floorY = supportHeight(body, previousLow, dt) + body.groundOffset;
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
        // Recorre los sólidos que afectan a este cuerpo.
        template <typename F>
        void forEachSolid(const Body& body, F f) const {
            for (const Box& box : obstacles) f(box);
            for (const Box& box : props) f(box);
            if (body.hitsCreatures) for (const Box& box : creatures) f(box);
        }

        // ¿Está el cuerpo (que empezó el paso con los pies en previousLow) por encima de la cara superior del sólido?
        static bool startedAbove(float previousLow, const Box& box) {
            return previousLow >= box.top() - STEP_HEIGHT;
        }

        // ¿Se solapa la huella circular del cuerpo con la caja en el plano horizontal?
        static bool overlapsFootprint(const Body& body, const Box& box) {
            const float dx = body.position.x - std::clamp(body.position.x, box.center.x - box.half.x, box.center.x + box.half.x);
            const float dz = body.position.z - std::clamp(body.position.z, box.center.z - box.half.z, box.center.z + box.half.z);
            return dx * dx + dz * dz <= body.collisionRadius * body.collisionRadius;
        }

        // Altura del suelo bajo el cuerpo: el terreno o la cara superior más alta de los sólidos que tiene debajo.
        // En los que no admiten quedarse encima (pokémon, personajes) el cuerpo se posa un instante y resbala.
        float supportHeight(Body& body, float previousLow, float dt) const {
            float height = groundHeight(body.position.x, body.position.z);
            if (body.collisionRadius <= 0.0f) return height;

            forEachSolid(body, [&](const Box& box) {
                if (!startedAbove(previousLow, box) || !overlapsFootprint(body, box)) return;
                height = (std::max)(height, box.top());
                if (!box.walkable) slideOff(body, box, dt);
            });
            return height;
        }

        static void slideOff(Body& body, const Box& box, float dt) {
            float nx = body.position.x - box.center.x;
            float nz = body.position.z - box.center.z;
            const float length = std::sqrt(nx * nx + nz * nz);
            if (length < 1.0e-4f) { nx = 1.0f; nz = 0.0f; } else { nx /= length; nz /= length; }
            body.position.x += nx * SLIDE_SPEED * dt;
            body.position.z += nz * SLIDE_SPEED * dt;
        }

        // Saca el cuerpo de los sólidos (en horizontal). Los proyectiles rebotan; el resto desliza.
        void pushOutAll(Body& body, float previousLow) const {
            const float high = body.position.y - body.groundOffset + body.collisionHeight;
            forEachSolid(body, [&](const Box& box) { pushOut(body, box, previousLow, high); });
        }

        static void pushOut(Body& body, const Box& box, float previousLow, float high) {
            if (startedAbove(previousLow, box) || high <= box.bottom()) return; // por encima o por debajo: no choca de lado

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
