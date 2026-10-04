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
    bool aimHold = false;     // apuntar mientras se mantiene (L2 en el mando)

    // Eventos de un solo frame (nueva pulsación).
    bool jump = false;
    bool crouch = false;      // agacharse / levantarse / deslizarse
    bool sprint = false;      // doble toque en W o pulsar L3
    bool pause = false;       // Options / Menú del mando
    bool throwBall = false;   // lanzar Pokéball: clic izquierdo / R2
    bool aimToggle = false;   // entrar / salir del modo lanzamiento: clic derecho
    bool escape = false;      // ESC: sale del modo lanzamiento o, si no, pausa
    bool lockTap = false;     // TAB / R3 (pulsación corta): fijar la cámara o cambiar de objetivo
    bool lockCancel = false;  // TAB / R3 mantenido: soltar el objetivo
};
