#pragma once

struct InputState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    // Desplazamiento del ratón (en píxeles) acumulado desde el frame anterior.
    float mouseDX = 0.0f;
    float mouseDY = 0.0f;
};