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
    MAP,           // mapa grande (la partida queda congelada)
    INVENTORY,     // mochila de objetos (la partida queda congelada)
    POKEDEX,       // Pokédex: todas las especies (la partida queda congelada)
    TYPES,         // tabla de tipos (la partida queda congelada)
    POKEMON,       // gestión de pokémon: equipo y PC (la partida queda congelada)
    STARTER,       // elección del pokémon inicial al empezar una partida nueva
    PLAYER,        // menú del jugador: nivel, medallas (la partida queda congelada)
    RESEARCH,      // máquina de investigación (la partida queda congelada)
    BATTLE,        // combate por turnos contra un entrenador (la partida queda congelada)
    SHOP,          // tienda del tendero (la partida queda congelada)
    SEEDS,         // elección de las semillas que se siembran en una parcela (la partida queda congelada)
    PC,            // PC de la máquina del inicio: único sitio donde se cambian pokémon entre equipo y PC (la partida queda congelada)
    UPDATING       // actualización de la aplicación en curso
};

// Estados en los que la escena 3D se dibuja de fondo.
constexpr bool isInGame(GameState state) {
    return state == GameState::PLAYING || state == GameState::PAUSED || state == GameState::CONTROLS || state == GameState::MAP ||
           state == GameState::INVENTORY || state == GameState::POKEDEX || state == GameState::TYPES || state == GameState::POKEMON || state == GameState::STARTER || state == GameState::RESEARCH || state == GameState::PC || state == GameState::PLAYER || state == GameState::SEEDS || state == GameState::BATTLE || state == GameState::SHOP;
}

// Estados en los que la ventana va en pantalla completa (desde que empieza la carga de la partida).
constexpr bool isFullscreen(GameState state) {
    return state == GameState::LOADING || isInGame(state);
}

// Estados que se redibujan continuamente; el resto solo con eventos.
constexpr bool isAnimated(GameState state) {
    return state == GameState::TITLE_SCREEN || state == GameState::LOADING || state == GameState::PLAYING || state == GameState::UPDATING || state == GameState::STARTER || state == GameState::BATTLE;
}

// Nombre de cada pantalla (registro y etapa del informe de cierres).
inline const char* gameStateName(GameState state) {
    switch (state) {
        case GameState::TITLE_SCREEN: return "TITLE_SCREEN";
        case GameState::MAIN_MENU: return "MAIN_MENU";
        case GameState::LOAD_MENU: return "LOAD_MENU";
        case GameState::REPLACE_MENU: return "REPLACE_MENU";
        case GameState::LOADING: return "LOADING";
        case GameState::PLAYING: return "PLAYING";
        case GameState::PAUSED: return "PAUSED";
        case GameState::CONTROLS: return "CONTROLS";
        case GameState::MAP: return "MAP";
        case GameState::INVENTORY: return "INVENTORY";
        case GameState::POKEDEX: return "POKEDEX";
        case GameState::TYPES: return "TYPES";
        case GameState::POKEMON: return "POKEMON";
        case GameState::STARTER: return "STARTER";
        case GameState::PLAYER: return "PLAYER";
        case GameState::RESEARCH: return "RESEARCH";
        case GameState::BATTLE: return "BATTLE";
        case GameState::SHOP: return "SHOP";
        case GameState::SEEDS: return "SEEDS";
        case GameState::PC: return "PC";
        case GameState::UPDATING: return "UPDATING";
    }
    return "?";
}
