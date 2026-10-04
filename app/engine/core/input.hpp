#pragma once

// Acciones del jugador de este frame, independientes del dispositivo (teclado, ratón o mando).
struct InputState {
    // Movimiento: strafe (-1 izquierda, +1 derecha) y avance (-1 atrás, +1 adelante).
    float moveX = 0.0f;
    float moveY = 0.0f;

    // Rotación de cámara de este frame, en "píxeles de ratón" equivalentes.
    float lookX = 0.0f;
    float lookY = 0.0f;

    // Estado mantenido.
    bool aim = false;       // apuntar: clic derecho / gatillo izquierdo (L2)

    // Eventos de un solo frame (nueva pulsación).
    bool jump = false;
    bool crouch = false;    // agacharse / levantarse / deslizarse
    bool sprint = false;    // doble toque en W o pulsar L3
    bool pause = false;     // Options / Menú del mando
    bool throwBall = false; // lanzar Pokéball: clic izquierdo / gatillo derecho (R2)
};
