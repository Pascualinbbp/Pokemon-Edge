#pragma once
#include "../rules/healthRules.hpp"
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../core/input.hpp"
#include "../../physics/physicsWorld.hpp"
#include "playerAction.hpp"
#include "playerPose.hpp"

// Controlador del personaje: decide su velocidad horizontal a partir de la entrada.
// La gravedad, el suelo y la integración del movimiento los resuelve Physics::World.
class Player {
    public:
    static constexpr float WALK_SPEED     = 5.0f;
    static constexpr float SPRINT_SPEED   = 9.0f;
    static constexpr float CROUCH_SPEED   = 2.5f;
    static constexpr float AIM_SPEED      = 3.5f;  // caminar mientras se apunta
    static constexpr float SLIDE_SPEED    = 10.5f; // algo más rápido que correr
    static constexpr float SLIDE_DURATION = 0.9f;  // segundos
    static constexpr float SLIDE_EASE_OUT = 0.2f;  // al final baja hasta la velocidad de correr
    static constexpr float JUMP_SPEED     = 7.0f;
    static constexpr float PLUNGE_MIN = 5.5f, PLUNGE_MIN_DEPTH = 0.8f, PLUNGE_KEEP = 0.7f, PLUNGE_MAX = 16.0f; // caída (m/s) a partir de la que se hunde, y cuánta velocidad conserva
    static constexpr float PLUNGE_TIME = 1.1f, PLUNGE_RISE = 3.5f, PLUNGE_HURT = 0.45f; // frenada, flotación y daño reducido al tocar el fondo
    float m_plungeTime = 0.0f, m_plungeSpeed = 0.0f;
    bool m_plunging = false;
    static constexpr float CLOUD_JUMP     = 1.45f; // el suelo de nube es blando: saltar en él rebota más
    bool onCloud = false;                          // lo fija la escena antes de cada actualización
    static constexpr float SWIM_SPEED     = 3.4f;  // nadar en la superficie
    static constexpr float DIVE_SPEED     = 4.6f;  // nadar sumergido (más rápido que en la superficie)
    static constexpr float SWIM_SPRINT    = 5.6f;  // nadar rápido (doble toque adelante, como al correr)
    static constexpr float DIVE_SPRINT    = 7.4f;
    static constexpr float SWIM_JUMP      = 5.0f;  // impulso al "saltar" nadando en la superficie
    static constexpr float DIVE_LOOK      = 0.35f; // mirando hacia abajo más que esto (rad) y avanzando, se sumerge
    static constexpr float DIVE_START_AIR = 0.4f;  // fracción de aire mínima para sumergirse mirando hacia abajo
    static constexpr float SPRINT_AIR     = 1.6f;  // bajo el agua, correr gasta el aire tantas veces más rápido
    static constexpr float STREAM_SPEED   = 18.0f; // velocidad con la que sube la corriente ascendente
    static constexpr float STREAM_ACCEL   = 30.0f;
    static constexpr float STREAM_LAUNCH  = 5.0f;  // impulso horizontal máximo al salir de la corriente
    static constexpr float SURFACE_RISE   = 2.5f;  // sin aire, el cuerpo sube solo a esa velocidad
    static constexpr float DIVE_MIN_DEPTH = 1.3f;  // hace falta al menos esta profundidad para sumergirse
    static constexpr float CLIMB_SPEED    = 3.0f;  // trepar una ladera muy empinada
    static constexpr float OXYGEN_REFILL  = 6.0f;  // el aire se recupera esta vez más rápido de lo que se gasta

    Physics::Body body;

    // Al apuntar el personaje se mueve siempre respecto a la cámara (la sigue de forma continua).
    // Lo asigna la escena antes de cada update.
    bool aiming = false;

    // Aire que aguanta el personaje bajo el agua (s). Es el punto de extensión del equipo de buceo: un equipamiento lo sube, y
    // con 'breathesUnderwater' no se gasta nunca.
    float oxygenCapacity = 14.0f;
    bool breathesUnderwater = false;

    Player() {
        body.stickToGround = true;
        body.collisionRadius = 0.4f;
        body.collisionHeight = 1.4f;
        body.hitsCreatures = true;
    }

    // Escala vertical del modelo según la postura (de pie, agachado o deslizándose).
    float heightScale() const { return m_heightScale; }

    // cameraLook: hacia dónde mira la cámara (rad, positivo hacia abajo): al bucear el cuerpo sigue esa dirección.
    void update(float dt, const InputState& input, float cameraYaw, float cameraLook, const Physics::World& world) {
        const bool wasGrounded = body.onGround;
        const bool forward = input.moveY > FORWARD_THRESHOLD;
        tickAction(dt);

        updateMoveBasis(input, cameraYaw);
        const bool streaming = updateStream(dt, world);
        m_grip = world.gripAt(body.position.x, body.position.z, body.feet());
        if (!streaming) liftByGeyser(dt, world);
        const bool inWater = !streaming && world.waterDepth(body.position.x, body.position.z, body.feet()) > Physics::World::SWIM_DEPTH;
        float dirX = 0.0f, dirZ = 0.0f;
        const float amount = direction(input.moveX, input.moveY, inWater ? cameraYaw : m_moveYaw, dirX, dirZ); // nadando se va hacia donde mira la cámara, y gira con ella
        if (streaming) {
            // la corriente lo lleva: no hay entrada que valga
        } else if (inWater) {
            updateWater(dt, input, cameraLook, world, dirX, dirZ, amount, forward);
        } else {
            leaveWater();
            updateSprint(input, wasGrounded, forward);
            updateStance(input, wasGrounded);
            updateSlide(dt, input);
            updateClimb(world, wasGrounded, dirX, dirZ, amount);
            updateHorizontal(dt, wasGrounded, dirX, dirZ, amount);
            if (wasGrounded && input.jump) jump(world); // también trepando: sirve para saltar antes de un precipicio o un hueco
        }

        trackFall(streaming);
        const float plungeBefore = body.velocity.y;
        if (!streaming) world.step(body, dt);
        if (!streaming && !inWater && !wasGrounded && body.onGround) { // al tocar el suelo: el agua poco profunda también amortigua
            m_impact = HealthRules::impactInWater(m_fallSpeed, columnDepth(world));
            land(input);
        }
        if (m_plunging && body.onGround && plungeBefore < -3.0f) { // la zambullida llega al fondo: solo ahí se hace daño, y reducido
            m_impact = -plungeBefore * PLUNGE_HURT;
            m_plungeTime = PLUNGE_TIME; // y a partir de ahí flota hacia arriba
        } else if (inWater && m_fallSpeed > 0.0f && !m_plunging && !m_diving) { // agua poco profunda: amortigua según lo que tiene
            m_impact = HealthRules::impactInWater(m_fallSpeed, columnDepth(world));
        }
        if (body.onGround || streaming || inWater) m_fallSpeed = 0.0f;

        updateFacing(dt, cameraYaw);
        m_heightScale = m_sliding ? SLIDE_HEIGHT : ((m_crouching || m_crouchQueued) ? CROUCH_HEIGHT : 1.0f);
        const float speed = std::hypot(body.velocity.x, body.velocity.z);
        m_speed = (std::min)(speed / WALK_SPEED, 1.0f);
        m_phase += dt * (3.0f + speed * 1.4f) * (speed > 0.05f ? 1.0f : 0.0f);
    }

    bool crouched() const { return m_crouching; }
    // Velocidad (m/s) con la que acaba de tocar el suelo tras caer; la lee y la borra quien calcule el daño.
    // Profundidad de toda la columna de agua bajo el jugador (del fondo a la superficie), no solo lo que lleva sumergido.
    float columnDepth(const Physics::World& world) const {
        const float x = body.position.x, z = body.position.z;
        return (std::max)(0.0f, world.waterLevel(x, z, body.feet()) - world.groundHeight(x, z));
    }
    float takeImpact() { const float v = m_impact; m_impact = 0.0f; return v; }
    // Sin aire con la cabeza bajo el agua (se ahoga).
    bool drowning() const { return m_drowning; }
    bool sliding() const { return m_sliding; }
    // Hacia dónde mira el personaje: hacia donde se mueve; al apuntar, trabajar o pescar, hacia donde mira la cámara.
    float facing() const { return m_facing; }

    // --- Estados ---
    bool climbing() const { return m_climbing; }
    bool swimming() const { return m_swimming; }
    bool diving() const { return m_diving; }
    bool streaming() const { return m_streaming; } // volando en una corriente ascendente
    float oxygen() const { return oxygenCapacity > 0.0f ? (std::clamp)(m_oxygen / oxygenCapacity, 0.0f, 1.0f) : 0.0f; }
    bool oxygenFull() const { return m_oxygen >= oxygenCapacity - 1.0e-3f; }
    bool exhausted() const { return m_exhausted; } // sin aire (recibe daño; el jugador decide)

    // --- Acciones (usar una herramienta...) ---
    // Empieza la animación de una acción (si ya hay una del mismo tipo a medias, la reinicia).
    void act(PlayerAction action) {
        m_action = action;
        m_actionTime = 0.0f;
        m_actionDuration = PlayerActions::duration(action);
    }
    PlayerAction action() const { return m_action; }
    // Acción que se mantiene en pose mientras no haya otra animación (la caña con el corcho en el agua).
    void hold(PlayerAction action) { m_holding = action; }

    PlayerPose pose() const {
        PlayerPose::Input in;
        in.action = m_action != PlayerAction::NONE ? m_action : m_holding;
        in.progress = m_action != PlayerAction::NONE && m_actionDuration > 0.0f ? m_actionTime / m_actionDuration : 1.0f;
        in.climbing = m_climbing;
        in.swimming = m_swimming && !m_diving;
        in.diving = m_diving;
        in.phase = m_phase;
        in.speed = m_climbing || m_swimming ? (std::max)(m_speed, 0.35f) : m_speed;
        in.crouched = m_crouching;
        return PlayerPose::make(in);
    }

    private:
    static constexpr float FORWARD_THRESHOLD = 0.3f;   // inclinación mínima del stick para contar como "avanzar"
    static constexpr float DIRECTION_JUMP = 0.35f;     // salto de la entrada entre dos frames que cuenta como "cambio de dirección"
    static constexpr float STEER_EPSILON = 0.05f;      // cambio mínimo de A/D o stick para girar el deslizamiento
    static constexpr float MOMENTUM_DECAY = 2.0f;      // pérdida de impulso por segundo en el aire
    static constexpr float TURN_SPEED = 14.0f;         // rad/s al girar el personaje
    static constexpr float FACING_MIN_SPEED = 0.3f;    // por debajo de esta velocidad no gira
    static constexpr float CROUCH_HEIGHT = 0.6f;
    static constexpr float SLIDE_HEIGHT = 0.45f;

    void updateFacing(float dt, float cameraYaw) {
        const bool faceCamera = aiming || m_action != PlayerAction::NONE || m_holding != PlayerAction::NONE;
        const float speed = std::hypot(body.velocity.x, body.velocity.z);
        float target = m_facing;
        if (faceCamera) target = cameraYaw;
        else if (speed > FACING_MIN_SPEED) target = std::atan2(body.velocity.x, body.velocity.z);
        const float delta = DirectX::XMScalarModAngle(target - m_facing);
        m_facing += (std::clamp)(delta, -TURN_SPEED * dt, TURN_SPEED * dt);
    }

    void tickAction(float dt) {
        if (m_action == PlayerAction::NONE) return;
        m_actionTime += dt;
        if (m_actionTime >= m_actionDuration) m_action = PlayerAction::NONE;
    }

    // La velocidad de caída máxima desde que se pierde el suelo (se descarta en el agua o en la corriente).
    void trackFall(bool streaming) {
        if (streaming) { m_fallSpeed = 0.0f; return; }
        if (!body.onGround) m_fallSpeed = (std::max)(m_fallSpeed, -body.velocity.y);
    }

    void leaveWater() {
        m_drowning = false;
        m_swimming = m_diving = m_exhausted = m_plunging = false;
        body.underwater = false;
        m_oxygen = oxygenCapacity;
    }

    // Corriente ascendente: sube con aceleración y, a medida que gana altura, se desliza hacia el destino (sin entrar en el terreno).
    // Al llegar arriba suelta al jugador con algo de impulso y cae con la física normal. Devuelve si sigue volando.
    bool updateStream(float dt, const Physics::World& world) {
        if (!m_streaming) {
            const Physics::World::Updraft* found = world.updraftAt(body.position.x, body.position.z);
            if (!found) return false;
            m_streaming = true;
            m_stream = *found;
            m_streamFrom = body.position;
            m_streamProgress = 0.0f;
            body.onGround = false;
            body.underwater = true;
            body.velocity.y = 0.0f;
        }
        m_swimming = m_diving = m_climbing = m_sprinting = m_sliding = m_crouching = m_crouchQueued = m_exhausted = m_drowning = m_plunging = false;
        m_oxygen = oxygenCapacity;
        body.velocity.y = (std::min)(STREAM_SPEED, body.velocity.y + STREAM_ACCEL * dt);
        body.position.y += body.velocity.y * dt;
        const float t = (std::clamp)((body.position.y - m_streamFrom.y) / (std::max)(m_stream.topY - m_streamFrom.y, 1.0f), 0.0f, 1.0f);
        const float wanted = (std::max)(m_streamProgress, t * t * t);
        const float x = m_streamFrom.x + (m_stream.toX - m_streamFrom.x) * wanted;
        const float z = m_streamFrom.z + (m_stream.toZ - m_streamFrom.z) * wanted;
        if (world.groundHeight(x, z) <= body.position.y + 0.5f) m_streamProgress = wanted; // no atraviesa la isla
        const float nx = m_streamFrom.x + (m_stream.toX - m_streamFrom.x) * m_streamProgress;
        const float nz = m_streamFrom.z + (m_stream.toZ - m_streamFrom.z) * m_streamProgress;
        body.velocity.x = (nx - body.position.x) / (std::max)(dt, 1.0e-4f);
        body.velocity.z = (nz - body.position.z) / (std::max)(dt, 1.0e-4f);
        body.position.x = nx;
        body.position.z = nz;
        if (body.position.y < m_stream.topY) return true;

        m_streaming = false;
        body.underwater = false;
        body.velocity.y = 0.0f;
        const float speed = std::hypot(body.velocity.x, body.velocity.z);
        if (speed > 1.0e-3f) { m_airDirX = body.velocity.x / speed; m_airDirZ = body.velocity.z / speed; }
        m_momentum = (std::min)(speed, STREAM_LAUNCH);
        return true;
    }

    // Nadar en la superficie o sumergirse: avanzando mientras la cámara mira hacia abajo (no hay tecla de bucear). Sumergido el cuerpo
    // sigue hacia donde mira la cámara. Correr también vale en el agua; "saltar" en la superficie da un impulso. Bajo el agua se gasta
    // aire; sin aire no sube solo: el jugador decide qué hacer y recibe daño.
    void updateWater(float dt, const InputState& input, float look, const Physics::World& world, float dirX, float dirZ, float amount, bool forward) {
        m_swimming = true;
        m_climbing = m_sliding = m_crouching = m_crouchQueued = false;
        updateSprint(input, true, forward);
        const float level = world.waterLevel(body.position.x, body.position.z, body.feet());
        const float depth = world.waterDepth(body.position.x, body.position.z, body.feet());
        const float surfaceY = level - body.collisionHeight * Physics::World::SUBMERGED;
        if (m_oxygen > oxygenCapacity) m_oxygen = oxygenCapacity;
        const bool lookingDown = forward && look > DIVE_LOOK && m_oxygen > DIVE_START_AIR * oxygenCapacity;
        if (m_fallSpeed > PLUNGE_MIN && !m_diving && depth >= PLUNGE_MIN_DEPTH) { // cae al agua desde alto: se hunde tanto más cuanto más rápido llegó
            m_plunging = true;
            m_plungeTime = 0.0f;
            m_plungeSpeed = -(std::min)(m_fallSpeed * PLUNGE_KEEP, PLUNGE_MAX);
            m_diving = true;
            body.onGround = false;
            body.velocity.y = m_plungeSpeed;
        }
        if (!m_diving && lookingDown && depth >= DIVE_MIN_DEPTH && !m_exhausted) {
            m_diving = true;
            body.onGround = false;
            body.velocity.y = -1.0f;
        }
        const float base = m_diving ? (m_sprinting ? DIVE_SPRINT : DIVE_SPEED) : (m_sprinting ? SWIM_SPRINT : SWIM_SPEED);
        float horizontal = base * amount;
        if (m_diving) {
            body.underwater = true;
            body.onGround = false;
            const float push = (std::clamp)(input.moveY, -1.0f, 1.0f);
            float vy = -std::sin(look) * base * push; // sin aire el jugador decide: solo recibe daño y el aviso
            if (m_plunging) { // zambullida: el impulso de la caída se frena en el agua y después flota hacia arriba
                m_plungeTime += dt;
                vy = m_plungeTime < PLUNGE_TIME ? m_plungeSpeed * (1.0f - m_plungeTime / PLUNGE_TIME) : PLUNGE_RISE;
                horizontal *= 0.4f;
            }
            horizontal *= 1.0f - std::fabs(push) * (1.0f - std::cos(look));
            body.velocity.y = vy;
            if (body.position.y >= surfaceY && vy >= 0.0f) { // volvió a la superficie
                m_diving = false;
                m_plunging = false;
                m_exhausted = false;
                body.underwater = false;
                body.position.y = surfaceY;
                body.velocity.y = 0.0f;
            }
        } else {
            body.underwater = false;
            if (input.jump && body.onGround) world.jump(body, SWIM_JUMP); // intenta flotar saltando
        }
        body.velocity.x = dirX * horizontal;
        body.velocity.z = dirZ * horizontal;

        const bool headUnder = m_diving && body.position.y + body.collisionHeight < level - 0.1f;
        m_drowning = headUnder && m_exhausted && !breathesUnderwater;
        if (headUnder && !breathesUnderwater) {
            m_oxygen -= dt * (m_sprinting ? SPRINT_AIR : 1.0f);
            if (m_oxygen <= 0.0f) {
                m_oxygen = 0.0f;
                m_exhausted = true;
            }
        } else {
            m_oxygen = (std::min)(oxygenCapacity, m_oxygen + dt * OXYGEN_REFILL);
        }
    }

    // Trepa las paredes casi verticales (desde CLIMB_SLOPE) en vez de andarlas: más lento y sin correr. Se puede saltar en cualquier momento.
    // Solo se sube trepando: bajar una ladera, aunque sea empinada, se anda (y bajo tierra se mira el suelo de la galería, no el terreno).
    void updateClimb(const Physics::World& world, bool grounded, float dirX, float dirZ, float amount) {
        const bool was = m_climbing;
        m_climbing = false;
        m_climbSlope = 0.0f;
        body.stickToGround = true;
        if (!grounded || m_sliding) return;
        // Dentro de una galería (o su boca) nunca se trepa: el terreno de encima es roca, no el suelo que se pisa.
        if (body.underground || world.inCave(body.position.x, body.position.z)) return;
        const float ahead = amount > 0.0f ? CLIMB_PROBE : 0.0f;
        const float px = body.position.x + dirX * ahead, pz = body.position.z + dirZ * ahead;
        if (world.inCave(px, pz)) return;
        float gx, gz;
        world.terrain.gradient(px, pz, gx, gz);
        const float steep = std::sqrt(gx * gx + gz * gz);
        const float along = gx * dirX + gz * dirZ;
        float fx, fz;
        world.terrain.gradient(px + dirX * CLIMB_SUSTAIN, pz + dirZ * CLIMB_SUSTAIN, fx, fz);
        const float limit = Physics::World::CLIMB_SLOPE * (was ? CLIMB_KEEP : 1.0f); // histéresis: no parpadea en el borde
        const bool wall = steep > limit && std::sqrt(fx * fx + fz * fz) > limit * 0.7f;
        m_climbSlope = amount > 0.0f ? along : 0.0f;
        // Solo se trepa hacia arriba. Cuesta abajo no hay escalada: si no es muy empinada se baja andando y, si lo es, el personaje deja de
        // pegarse al suelo y cae (con su daño de caída si la altura lo merece).
        if (wall && amount > 0.0f && along < -steep * 0.35f) body.stickToGround = false;
        m_climbing = wall && amount > 0.0f && along > steep * 0.35f;
        if (!m_climbing) return;
        m_climbSteep = steep;
        m_sprinting = false;
        m_crouching = false;
    }

    // La cámara orbita libremente alrededor del jugador: girarla NO desvía al personaje.
    // El ángulo de referencia del movimiento (m_moveYaw) solo se toma de la cámara:
    //   - mientras no hay entrada (así la siguiente vez que se empieza a andar se usa la vista actual),
    //   - cuando la entrada cambia de golpe (pulsar o soltar una tecla, mover el stick de un lado a otro),
    //   - y continuamente mientras se apunta.
    // Con la entrada estable (tecla mantenida) o cambiando poco a poco (stick), la referencia no se toca,
    // así que el personaje mantiene su rumbo aunque se mueva la cámara.
    void updateMoveBasis(const InputState& input, float cameraYaw) {
        const bool idle = input.moveX == 0.0f && input.moveY == 0.0f;
        const float jump = std::hypot(input.moveX - m_prevMoveX, input.moveY - m_prevMoveY);
        if (aiming || idle || jump > DIRECTION_JUMP) m_moveYaw = cameraYaw;
        m_prevMoveX = input.moveX;
        m_prevMoveY = input.moveY;
    }

    // Dirección unitaria en XZ para unos ejes de movimiento y un ángulo de referencia.
    // Devuelve cuánto se empuja (0..1); 0 si no hay entrada. Las diagonales quedan normalizadas.
    static float direction(float strafe, float forward, float yaw, float& x, float& z) {
        const float length = std::sqrt(strafe * strafe + forward * forward);
        if (length == 0.0f) return 0.0f;

        float s, c;
        DirectX::XMScalarSinCos(&s, &c, yaw);
        // Adelante = (s, c), derecha = (c, -s).
        x = (forward * s + strafe * c) / length;
        z = (forward * c - strafe * s) / length;
        return (std::min)(length, 1.0f);
    }

    // Correr: doble toque en W / L3, solo desde el suelo. Termina al dejar de avanzar o al apuntar.
    void updateSprint(const InputState& input, bool grounded, bool forward) {
        if (!forward || aiming) {
            m_sprinting = false;
        } else if (input.sprint && grounded) {
            m_sprinting = true;
            m_crouching = false;
        }
    }

    // Una pulsación de agacharse: corriendo desliza; en cualquier otro caso agacha o levanta.
    // En el aire se guarda para el aterrizaje.
    void updateStance(const InputState& input, bool grounded) {
        if (!input.crouch) return;
        if (!grounded) {
            m_crouchQueued = true;
        } else if (m_sliding) {
            return;
        } else if (m_sprinting) {
            startSlide(input.moveX);
        } else {
            m_crouching = !m_crouching;
        }
    }

    void startSlide(float strafe) {
        m_sliding = true;
        m_crouching = false;
        m_slideTime = 0.0f;
        m_slideStrafe = strafe;
        direction(strafe, 1.0f, m_moveYaw, m_slideDirX, m_slideDirZ);
    }

    // El deslizamiento siempre avanza; A/D (o el stick) giran la dirección. La cámara no la cambia:
    // solo se recalcula cuando el jugador cambia su dirección lateral.
    void updateSlide(float dt, const InputState& input) {
        if (!m_sliding) return;

        m_slideTime += dt;
        if (m_slideTime >= SLIDE_DURATION) {
            m_sliding = false; // vuelve a la postura normal (y sigue corriendo si sigue avanzando)
        } else if (std::fabs(input.moveX - m_slideStrafe) > STEER_EPSILON) {
            m_slideStrafe = input.moveX;
            direction(m_slideStrafe, 1.0f, m_moveYaw, m_slideDirX, m_slideDirZ);
        }
    }

    // Velocidad constante durante el deslizamiento; en el último tramo baja a la de correr.
    float slideSpeed() const {
        const float remaining = (std::clamp)((SLIDE_DURATION - m_slideTime) / SLIDE_EASE_OUT, 0.0f, 1.0f);
        return SPRINT_SPEED + (SLIDE_SPEED - SPRINT_SPEED) * remaining;
    }

    // Velocidad al caminar por el suelo según la postura.
    float groundSpeed() const {
        if (m_sprinting) return SPRINT_SPEED;
        if (m_crouching) return CROUCH_SPEED;
        return aiming ? AIM_SPEED : WALK_SPEED;
    }

    // Asigna la velocidad horizontal del cuerpo (la física la integra después).
    void updateHorizontal(float dt, bool grounded, float dirX, float dirZ, float amount) {
        float speed = 0.0f;
        if (m_sliding) {
            dirX = m_slideDirX;
            dirZ = m_slideDirZ;
            speed = slideSpeed();
        } else if (grounded) {
            if (m_climbing) speed = CLIMB_SPEED * amount / std::sqrt(1.0f + m_climbSteep * m_climbSteep); // velocidad constante sobre la pared
            else if (amount > 0.0f) speed = groundSpeed() * amount;
        } else {
            // En el aire se conserva el impulso (por ejemplo, el de un salto desde un deslizamiento).
            if (amount > 0.0f) {
                m_airDirX = dirX;
                m_airDirZ = dirZ;
                speed = (std::max)((m_sprinting ? SPRINT_SPEED : WALK_SPEED) * amount, m_momentum);
            } else {
                speed = m_momentum;
            }
            m_momentum = (std::max)(m_momentum - MOMENTUM_DECAY * dt, 0.0f);
            dirX = m_airDirX;
            dirZ = m_airDirZ;
        }

        if (grounded && !m_sliding && m_grip < 0.99f) { // hielo: la velocidad cambia despacio y se patina
            const float k = (std::min)(1.0f, dt * m_grip * ICE_RESPONSE);
            body.velocity.x += (dirX * speed - body.velocity.x) * k;
            body.velocity.z += (dirZ * speed - body.velocity.z) * k;
            return;
        }
        body.velocity.x = dirX * speed;
        body.velocity.z = dirZ * speed;
    }

    // El chorro de un géiser en erupción lanza hacia arriba a quien está encima (sin superar la altura del chorro).
    void liftByGeyser(float dt, const Physics::World& world) {
        float level = 0.0f;
        const Physics::World::Geyser* geyser = world.geyserAt(body.position.x, body.position.z, level);
        if (!geyser) return;
        const float top = world.groundHeight(geyser->x, geyser->z) + geyser->height * level;
        if (body.position.y >= top) return;
        body.onGround = false;
        body.velocity.y = (std::min)(body.velocity.y + GEYSER_LIFT * dt, std::sqrt(2.0f * Physics::World::GRAVITY * (std::max)(top - body.position.y, 0.5f)));
    }

    void jump(const Physics::World& world) {
        world.jump(body, JUMP_SPEED * (onCloud ? CLOUD_JUMP : 1.0f));
        m_crouching = false;
        if (m_sliding) { // saltar desde un deslizamiento conserva el impulso de carrera
            m_sliding = false;
            m_momentum = (std::max)(slideSpeed(), SPRINT_SPEED);
            m_airDirX = m_slideDirX;
            m_airDirZ = m_slideDirZ;
        }
    }

    // Al aterrizar se aplica la pulsación de agacharse hecha en el aire: corriendo desliza, si no, agacha.
    void land(const InputState& input) {
        m_momentum = 0.0f;
        if (!m_crouchQueued) return;

        m_crouchQueued = false;
        if (m_sprinting) startSlide(input.moveX);
        else m_crouching = true;
    }

    static constexpr float ICE_RESPONSE = 9.0f;        // rapidez con la que la velocidad sigue a la entrada en hielo (se multiplica por el agarre)
    static constexpr float GEYSER_LIFT = 70.0f;        // aceleración (m/s²) del chorro de un géiser
    static constexpr float CLIMB_SUSTAIN = 1.2f;       // a qué distancia más allá debe seguir la pared
    static constexpr float CLIMB_KEEP = 0.75f;         // fracción del umbral con la que se sigue trepando una vez empezado
    static constexpr float CLIMB_PROBE = 0.45f;        // a qué distancia por delante se mira la pendiente

    PlayerAction m_action = PlayerAction::NONE;
    float m_facing = 0.0f;
    PlayerAction m_holding = PlayerAction::NONE;
    float m_actionTime = 0.0f, m_actionDuration = 0.0f;
    float m_oxygen = 14.0f;
    float m_fallSpeed = 0.0f, m_impact = 0.0f, m_grip = 1.0f;
    bool m_drowning = false;
    float m_phase = 0.0f, m_speed = 0.0f, m_climbSlope = 0.0f, m_climbSteep = 0.0f;
    bool m_streaming = false;
    Physics::World::Updraft m_stream;
    DirectX::XMFLOAT3 m_streamFrom = { 0.0f, 0.0f, 0.0f };
    float m_streamProgress = 0.0f;
    bool m_climbing = false, m_swimming = false, m_diving = false, m_exhausted = false;
    float m_moveYaw = 0.0f;     // ángulo de referencia del movimiento (el de la cámara cuando cambió la entrada)
    float m_prevMoveX = 0.0f;   // entrada del frame anterior
    float m_prevMoveY = 0.0f;
    float m_slideTime = 0.0f;
    float m_slideStrafe = 0.0f;
    float m_slideDirX = 0.0f;
    float m_slideDirZ = 0.0f;
    float m_airDirX = 0.0f;
    float m_airDirZ = 0.0f;
    float m_momentum = 0.0f;
    float m_heightScale = 1.0f;
    bool m_sprinting = false;
    bool m_sliding = false;
    bool m_crouching = false;
    bool m_crouchQueued = false;
};
