#pragma once

struct InputState {
    // Teclas mantenidas.
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool crouch = false;

    // Eventos de un solo frame (se consumen en cada lectura).
    bool jump = false;   // nueva pulsación de espacio
    bool sprint = false; // doble toque en W

    // Desplazamiento del ratón (en píxeles) acumulado desde el frame anterior.
    float mouseDX = 0.0f;
    float mouseDY = 0.0f;
};