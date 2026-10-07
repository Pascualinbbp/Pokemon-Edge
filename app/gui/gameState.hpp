#pragma once

enum class GameState {
    TITLE_SCREEN,
    MAIN_MENU,
    LOAD_MENU,     // lista de partidas guardadas
    REPLACE_MENU,  // 4 partidas guardadas: elegir cuál eliminar para empezar una nueva
    LOADING,
    PLAYING,
    PAUSED,
    CONTROLS,
    INVENTORY,     // mochila de objetos (la partida queda congelada)
    POKEMON,       // gestión de pokémon: equipo y PC (la partida queda congelada)
    STARTER,       // elección del pokémon inicial al empezar una partida nueva
    RESEARCH,      // máquina de investigación (la partida queda congelada)
    UPDATING       // actualización de la aplicación en curso
};

// Estados en los que la escena 3D se dibuja de fondo.
constexpr bool isInGame(GameState state) {
    return state == GameState::PLAYING || state == GameState::PAUSED || state == GameState::CONTROLS ||
           state == GameState::INVENTORY || state == GameState::POKEMON || state == GameState::STARTER || state == GameState::RESEARCH;
}

// Estados en los que la ventana va en pantalla completa (desde que empieza la carga de la partida).
constexpr bool isFullscreen(GameState state) {
    return state == GameState::LOADING || isInGame(state);
}

// Estados que se redibujan continuamente; el resto solo con eventos.
constexpr bool isAnimated(GameState state) {
    return state == GameState::TITLE_SCREEN || state == GameState::LOADING || state == GameState::PLAYING || state == GameState::UPDATING;
}
