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
    bool inventory = false;   // I / Select (View en Xbox): abrir el inventario
    bool map = false;         // M / cruceta arriba: abrir el mapa
    bool pokedex = false;     // P / cruceta derecha: abrir la Pokédex
    bool modeSwitch = false;  // R / triángulo (Y en Xbox): cambiar el modo del pokémon que acompaña
    bool interact = false;    // F / cuadrado (X en Xbox): abrir un cofre cercano
    bool interactHeld = false; // F / cuadrado mantenido: sigue trabajando el recurso cercano
    int ballSwitch = 0;       // Q / E / rueda del ratón / L1 / R1: -1 pokéball (o pokémon) anterior, +1 siguiente
    int teamSelect = 0;       // teclas 1..6: pokémon del equipo que pasa a acompañar al jugador (0 = ninguna)
};
