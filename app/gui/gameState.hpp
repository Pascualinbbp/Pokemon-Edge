#pragma once

enum class GameState {
    TITLE_SCREEN,
    MAIN_MENU,
    LOADING,
    PLAYING,
    PAUSED,
    CONTROLS
};

// Estados en los que la escena 3D se dibuja de fondo.
constexpr bool isInGame(GameState state) {
    return state == GameState::PLAYING || state == GameState::PAUSED || state == GameState::CONTROLS;
}

// Estados en los que la ventana va en pantalla completa (desde que empieza la carga de la partida).
constexpr bool isFullscreen(GameState state) {
    return state != GameState::TITLE_SCREEN && state != GameState::MAIN_MENU;
}

// Estados que se redibujan continuamente; el resto solo con eventos.
constexpr bool isAnimated(GameState state) {
    return state == GameState::TITLE_SCREEN || state == GameState::LOADING || state == GameState::PLAYING;
}