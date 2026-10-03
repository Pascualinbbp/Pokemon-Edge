#pragma once

// Acciones del jugador de este frame, independientes del dispositivo (teclado, ratón o mando).
struct InputState {
    // Movimiento: strafe (-1 izquierda, +1 derecha) y avance (-1 atrás, +1 adelante).
    float moveX = 0.0f;
    float moveY = 0.0f;

    // Rotación de cámara de este frame, en "píxeles de ratón" equivalentes.
    float lookX = 0.0f;
    float lookY = 0.0f;

    bool crouch = false; // mantenido

    // Eventos de un solo frame.
    bool jump = false;   // nueva pulsación de salto
    bool sprint = false; // doble toque en W o pulsar L3
};