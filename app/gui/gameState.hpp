#pragma once

enum class GameState {
    TITLE_SCREEN,
    MAIN_MENU,
    PLAYING,
    PAUSED,
    CONTROLS
};

// Estados en los que la escena 3D se dibuja de fondo.
constexpr bool isInGame(GameState state) {
    return state == GameState::PLAYING || state == GameState::PAUSED || state == GameState::CONTROLS;
}