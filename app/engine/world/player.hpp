#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../core/input.hpp"
#include "../physics/physicsWorld.hpp"

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

    Physics::Body body;

    // Al apuntar el personaje se mueve siempre respecto a la cámara (la sigue de forma continua).
    // Lo asigna la escena antes de cada update.
    bool aiming = false;

    Player() { body.shadowRadius = SHADOW_RADIUS; }

    // Escala vertical del modelo según la postura (de pie, agachado o deslizándose).
    float heightScale() const { return m_heightScale; }

    void update(float dt, const InputState& input, float cameraYaw, const Physics::World& world) {
        const bool wasGrounded = body.onGround;
        const bool forward = input.moveY > FORWARD_THRESHOLD;

        updateMoveBasis(input, cameraYaw);
        float dirX = 0.0f, dirZ = 0.0f;
        const float amount = direction(input.moveX, input.moveY, m_moveYaw, dirX, dirZ);

        updateSprint(input, wasGrounded, forward);
        updateStance(input, wasGrounded);
        updateSlide(dt, input);
        updateHorizontal(dt, wasGrounded, dirX, dirZ, amount);
        if (wasGrounded && input.jump) jump(world);

        world.step(body, dt);
        if (!wasGrounded && body.onGround) land(input);

        m_heightScale = m_sliding ? SLIDE_HEIGHT : ((m_crouching || m_crouchQueued) ? CROUCH_HEIGHT : 1.0f);
    }

    private:
    static constexpr float FORWARD_THRESHOLD = 0.3f;   // inclinación mínima del stick para contar como "avanzar"
    static constexpr float DIRECTION_JUMP = 0.35f;     // salto de la entrada entre dos frames que cuenta como "cambio de dirección"
    static constexpr float STEER_EPSILON = 0.05f;      // cambio mínimo de A/D o stick para girar el deslizamiento
    static constexpr float MOMENTUM_DECAY = 2.0f;      // pérdida de impulso por segundo en el aire
    static constexpr float CROUCH_HEIGHT = 0.6f;
    static constexpr float SLIDE_HEIGHT = 0.45f;
    static constexpr float SHADOW_RADIUS = 0.55f;

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
            if (amount > 0.0f) speed = groundSpeed() * amount;
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
