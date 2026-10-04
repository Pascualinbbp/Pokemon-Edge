#pragma once

// Estado del juego que la interfaz necesita mostrar (el HUD no conoce la escena).
struct GameStatus {
    float aimBlend = 0.0f;       // 0 = cámara normal, 1 = cámara de apuntado completa
    int captures = 0;            // capturas de esta sesión
    bool captureNotice = false;  // mostrar el aviso de "¡Capturado!"
};
