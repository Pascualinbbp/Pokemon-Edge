#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "body.hpp"
#include "heightfield.hpp"

namespace Physics {
    // Reglas físicas compartidas por todas las entidades del mundo.
    class World {
        public:
        static constexpr float GRAVITY = 20.0f;
        static constexpr float HALF_SIZE = 384.0f; // las mallas del mundo cubren de -HALF_SIZE a +HALF_SIZE; el mundo jugable es el archipiélago y las regiones
        static constexpr float CLIMB_SLOPE = 0.85f; // pendiente (m de subida por m) desde la que no se anda: se escala
        static constexpr float CLIMB_MAX = 2.4f;    // más empinado que esto es una pared: no se puede subir
        static constexpr float SWIM_DEPTH = 0.6f;  // con más agua que esto no se camina: se nada
        static constexpr float START_HEIGHT = 1.0f; // altura del terreno plano de la zona de inicio (paredes, máquina...)
        static constexpr float SUBMERGED = 0.4f;   // fracción de la altura de un cuerpo que se hunde al flotar
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

        // Zona jugable: un disco (el archipiélago) más las regiones especiales (cueva, mina, isla del cielo).
        // trigger: a menos de esta distancia del centro el punto queda atado al disco (0 = el radio más 40 m).
        struct Disc { float x = 0.0f, z = 0.0f, radius = 0.0f, trigger = 0.0f; };

        // Corriente ascendente: una columna que sube hasta topY y deja caer en (toX, toZ).
        struct Updraft { float x = 0.0f, z = 0.0f, radius = 6.0f, topY = 0.0f, toX = 0.0f, toZ = 0.0f; };

        Heightfield terrain;  // relieve del mundo (lo rellena la escena al generarlo)
        Heightfield water;    // altura de la superficie del agua (= el terreno donde no hay agua)
        Heightfield seaMask;  // 1 donde el agua es mar (sube y baja con la marea)
        Heightfield flowX, flowZ; // corriente de los ríos (m/s)
        float tide = 0.0f;    // desviación de la marea (m), la fija la escena cada frame
        Disc archipelago{ 0.0f, 0.0f, HALF_SIZE };
        std::vector<Disc> regions;
        std::vector<Updraft> updrafts;

        // Altura del terreno en (x, z) (bajo el agua es el fondo).
        float groundHeight(float x, float z) const { return terrain.sample(x, z); }
        // Altura de la superficie del agua en (x, z) (con la marea); en tierra seca es la del terreno.
        float waterLevel(float x, float z) const { return water.empty() ? 0.0f : water.sample(x, z) + tide * seaMask.sample(x, z); }
        // Altura de lo que se ve: el terreno o la superficie del agua.
        float surfaceHeight(float x, float z) const { return (std::max)(groundHeight(x, z), waterLevel(x, z)); }
        // Profundidad del agua en (x, z) (0 = tierra).
        float waterDepth(float x, float z) const { return (std::max)(0.0f, waterLevel(x, z) - groundHeight(x, z)); }
        bool swimmable(float x, float z) const { return waterDepth(x, z) > SWIM_DEPTH; }
        // Pendiente del terreno (m/m).
        float slope(float x, float z) const { return terrain.slope(x, z); }
        // Corriente del agua en un punto (0 donde no hay).
        void current(float x, float z, float& fx, float& fz) const {
            fx = flowX.empty() ? 0.0f : flowX.sample(x, z);
            fz = flowZ.empty() ? 0.0f : flowZ.sample(x, z);
        }

        // Corriente ascendente que abarca el punto (nullptr = ninguna).
        const Updraft* updraftAt(float x, float z) const {
            for (const Updraft& u : updrafts) {
                const float dx = x - u.x, dz = z - u.z;
                if (dx * dx + dz * dz <= u.radius * u.radius) return &u;
            }
            return nullptr;
        }

        // Deja un punto dentro de la zona jugable: la región en la que está o, si no, el archipiélago.
        void clampToWorld(float& x, float& z, float margin = 2.0f) const {
            for (const Disc& region : regions) {
                const float dx = x - region.x, dz = z - region.z;
                const float reach = region.trigger > 0.0f ? region.trigger : region.radius + 40.0f;
                if (dx * dx + dz * dz > reach * reach) continue;
                limit(x, z, region, margin);
                return;
            }
            limit(x, z, archipelago, margin);
        }

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
            if (!body.onGround && !body.underwater) body.velocity.y -= GRAVITY * body.gravityScale * dt;
            if (body.rolls && body.onGround && !body.underwater && waterDepth(body.position.x, body.position.z) <= 0.05f) { // rueda cuesta abajo
                float gx, gz;
                terrain.gradient(body.position.x, body.position.z, gx, gz);
                const float pull = GRAVITY * body.gravityScale * ROLL_FACTOR * dt;
                body.velocity.x -= gx * pull;
                body.velocity.z -= gz * pull;
            }

            body.position.x += body.velocity.x * dt;
            body.position.z += body.velocity.z * dt;
            body.position.y += body.velocity.y * dt;
            const bool afloat = waterDepth(body.position.x, body.position.z) > 0.05f;
            if (afloat) { // la corriente arrastra
                float fx, fz;
                current(body.position.x, body.position.z, fx, fz);
                body.position.x += fx * dt;
                body.position.z += fz * dt;
            }
            clampToWorld(body.position.x, body.position.z);
            if (body.collisionRadius > 0.0f) pushOutAll(body, previousLow);

            const float floorY = supportHeight(body, previousLow, dt) + body.groundOffset;
            if (body.stickToGround && body.onGround && !body.underwater && body.velocity.y <= 0.0f && body.position.y - floorY < STICK_DISTANCE) body.position.y = floorY;
            const bool touching = body.position.y <= floorY && body.velocity.y <= 0.0f;
            body.onGround = touching;
            if (!touching) return;

            body.position.y = floorY;

            // Sobre el agua no hay bote: el cuerpo se posa y flota.
            const bool onWater = waterDepth(body.position.x, body.position.z) > 0.05f;
            if (onWater) {
                body.velocity.y = 0.0f;
                const float keep = (std::max)(0.0f, 1.0f - WATER_DRAG * dt);
                body.velocity.x *= keep;
                body.velocity.z *= keep;
                return;
            }

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
        static void limit(float& x, float& z, const Disc& disc, float margin) {
            const float dx = x - disc.x, dz = z - disc.z, reach = disc.radius - margin;
            const float d2 = dx * dx + dz * dz;
            if (d2 <= reach * reach) return;
            const float k = reach / std::sqrt(d2);
            x = disc.x + dx * k;
            z = disc.z + dz * k;
        }

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
            // Sobre el agua los cuerpos flotan, algo hundidos según su altura.
            float height = groundHeight(body.position.x, body.position.z);
            if (!body.underwater) height = (std::max)(height, waterLevel(body.position.x, body.position.z) - body.collisionHeight * SUBMERGED);
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

        static constexpr float ROLL_FACTOR = 0.6f;      // fracción de la gravedad que empuja a lo que rueda por una cuesta
        static constexpr float BOUNCE_MIN_SPEED = 1.5f; // por debajo de esta velocidad de caída ya no bota
        static constexpr float STICK_DISTANCE = 0.9f;  // cuánto puede bajar el suelo de un paso a otro sin que el personaje se despegue
        static constexpr float WATER_DRAG = 3.0f;       // frenado horizontal por segundo de lo que flota
        static constexpr float BOUNCE_FRICTION = 0.8f;  // fracción de velocidad horizontal que se conserva en cada bote
    };
}
