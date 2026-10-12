#pragma once
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
    static constexpr float SWIM_SPEED     = 3.4f;  // nadar en la superficie
    static constexpr float DIVE_SPEED     = 2.8f;  // nadar sumergido
    static constexpr float DIVE_VERTICAL  = 2.2f;  // subir / bajar al bucear
    static constexpr float SURFACE_RISE   = 4.0f;  // sin aire, el cuerpo sube solo a esa velocidad
    static constexpr float DIVE_MIN_DEPTH = 1.3f;  // hace falta al menos esta profundidad para sumergirse
    static constexpr float CLIMB_SPEED    = 2.0f;  // trepar una ladera muy empinada
    static constexpr float DESCEND_SPEED  = 3.0f;  // bajarla
    static constexpr float OXYGEN_REFILL  = 6.0f;  // el aire se recupera esta vez más rápido de lo que se gasta

    Physics::Body body;

    // Al apuntar el personaje se mueve siempre respecto a la cámara (la sigue de forma continua).
    // Lo asigna la escena antes de cada update.
    bool aiming = false;

    // Aire que aguanta el personaje bajo el agua (s). Es el punto de extensión del equipo de buceo: un equipamiento lo sube, y
    // con 'breathesUnderwater' no se gasta nunca.
    float oxygenCapacity = 30.0f;
    bool breathesUnderwater = false;

    Player() {
        body.stickToGround = true;
        body.collisionRadius = 0.4f;
        body.collisionHeight = 1.4f;
        body.hitsCreatures = true;
    }

    // Escala vertical del modelo según la postura (de pie, agachado o deslizándose).
    float heightScale() const { return m_heightScale; }

    void update(float dt, const InputState& input, float cameraYaw, const Physics::World& world) {
        const bool wasGrounded = body.onGround;
        const bool forward = input.moveY > FORWARD_THRESHOLD;
        tickAction(dt);

        updateMoveBasis(input, cameraYaw);
        float dirX = 0.0f, dirZ = 0.0f;
        const float amount = direction(input.moveX, input.moveY, m_moveYaw, dirX, dirZ);

        const bool inWater = world.waterDepth(body.position.x, body.position.z) > Physics::World::SWIM_DEPTH;
        if (inWater) {
            updateWater(dt, input, world, dirX, dirZ, amount);
        } else {
            leaveWater();
            updateSprint(input, wasGrounded, forward);
            updateStance(input, wasGrounded);
            updateSlide(dt, input);
            updateClimb(world, wasGrounded, dirX, dirZ, amount);
            updateHorizontal(dt, wasGrounded, dirX, dirZ, amount);
            if (wasGrounded && input.jump && !m_climbing) jump(world);
        }

        world.step(body, dt);
        if (!inWater && !wasGrounded && body.onGround) land(input);

        updateFacing(dt, cameraYaw);
        m_heightScale = m_sliding ? SLIDE_HEIGHT : ((m_crouching || m_crouchQueued) ? CROUCH_HEIGHT : 1.0f);
        const float speed = std::hypot(body.velocity.x, body.velocity.z);
        m_speed = (std::min)(speed / WALK_SPEED, 1.0f);
        m_phase += dt * (3.0f + speed * 1.4f) * (speed > 0.05f ? 1.0f : 0.0f);
    }

    bool crouched() const { return m_crouching; }
    // Hacia dónde mira el personaje: hacia donde se mueve; al apuntar, trabajar o pescar, hacia donde mira la cámara.
    float facing() const { return m_facing; }

    // --- Estados ---
    bool climbing() const { return m_climbing; }
    bool swimming() const { return m_swimming; }
    bool diving() const { return m_diving; }
    float oxygen() const { return oxygenCapacity > 0.0f ? (std::clamp)(m_oxygen / oxygenCapacity, 0.0f, 1.0f) : 0.0f; }
    bool oxygenFull() const { return m_oxygen >= oxygenCapacity - 1.0e-3f; }
    bool exhausted() const { return m_exhausted; } // sin aire: sube solo a la superficie

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

    void leaveWater() {
        m_swimming = m_diving = m_exhausted = false;
        body.underwater = false;
        m_oxygen = oxygenCapacity;
    }

    // Nadar en la superficie o, con la tecla de agacharse, sumergirse: espacio sube, shift baja. Bajo el agua se gasta aire;
    // sin aire el cuerpo sube solo a la superficie.
    void updateWater(float dt, const InputState& input, const Physics::World& world, float dirX, float dirZ, float amount) {
        m_swimming = true;
        m_climbing = m_sprinting = m_sliding = m_crouching = m_crouchQueued = false;
        const float level = world.waterLevel(body.position.x, body.position.z);
        const float depth = world.waterDepth(body.position.x, body.position.z);
        const float surfaceY = level - body.collisionHeight * Physics::World::SUBMERGED;
        if (m_oxygen > oxygenCapacity) m_oxygen = oxygenCapacity;
        if (!m_diving && input.crouch && depth >= DIVE_MIN_DEPTH && !m_exhausted) {
            m_diving = true;
            body.onGround = false;
            body.velocity.y = -1.0f;
        }
        if (m_diving) {
            body.underwater = true;
            body.onGround = false;
            float vy = 0.0f;
            if (m_exhausted) vy = SURFACE_RISE;
            else if (input.jumpHeld) vy = DIVE_VERTICAL;
            else if (input.crouchHeld) vy = -DIVE_VERTICAL;
            body.velocity.y = vy;
            if (body.position.y >= surfaceY && vy >= 0.0f) { // volvió a la superficie
                m_diving = false;
                m_exhausted = false;
                body.underwater = false;
                body.position.y = surfaceY;
                body.velocity.y = 0.0f;
            }
        } else {
            body.underwater = false;
        }
        const float speed = (m_diving ? DIVE_SPEED : SWIM_SPEED) * amount;
        body.velocity.x = dirX * speed;
        body.velocity.z = dirZ * speed;

        const bool headUnder = m_diving && body.position.y + body.collisionHeight < level - 0.1f;
        if (headUnder && !breathesUnderwater) {
            m_oxygen -= dt;
            if (m_oxygen <= 0.0f) {
                m_oxygen = 0.0f;
                m_exhausted = true;
            }
        } else {
            m_oxygen = (std::min)(oxygenCapacity, m_oxygen + dt * OXYGEN_REFILL);
        }
    }

    // Trepa las laderas muy empinadas en vez de andarlas: más lento, sin correr ni saltar; más allá de CLIMB_MAX es una pared.
    void updateClimb(const Physics::World& world, bool grounded, float dirX, float dirZ, float amount) {
        m_climbing = false;
        m_blocked = false;
        m_climbSlope = 0.0f;
        if (!grounded || m_sliding) return;
        const float ahead = amount > 0.0f ? CLIMB_PROBE : 0.0f;
        float gx, gz;
        world.terrain.gradient(body.position.x + dirX * ahead, body.position.z + dirZ * ahead, gx, gz);
        const float along = gx * dirX + gz * dirZ; // subida por metro en la dirección de avance
        const float steepHere = std::sqrt(gx * gx + gz * gz);
        m_climbSlope = amount > 0.0f ? along : 0.0f;
        m_climbing = steepHere > Physics::World::CLIMB_SLOPE && (amount <= 0.0f || std::fabs(along) > Physics::World::CLIMB_SLOPE * 0.8f);
        if (!m_climbing) return;
        m_sprinting = false;
        m_crouching = false;
        m_blocked = amount > 0.0f && along > Physics::World::CLIMB_MAX;
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
            if (m_climbing) speed = m_blocked ? 0.0f : (m_climbSlope < 0.0f ? DESCEND_SPEED : CLIMB_SPEED) * amount;
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

        body.velocity.x = dirX * speed;
        body.velocity.z = dirZ * speed;
    }

    void jump(const Physics::World& world) {
        world.jump(body, JUMP_SPEED);
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

    static constexpr float CLIMB_PROBE = 0.45f;        // a qué distancia por delante se mira la pendiente

    PlayerAction m_action = PlayerAction::NONE;
    float m_facing = 0.0f;
    PlayerAction m_holding = PlayerAction::NONE;
    float m_actionTime = 0.0f, m_actionDuration = 0.0f;
    float m_oxygen = 30.0f;
    float m_phase = 0.0f, m_speed = 0.0f, m_climbSlope = 0.0f;
    bool m_climbing = false, m_blocked = false, m_swimming = false, m_diving = false, m_exhausted = false;
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
