#pragma once
#include <windows.h>

enum class GameState {
    TITLE_SCREEN,
    MAIN_MENU
};

namespace MainWindow {
    void init();
    void run();
    void cleanup();
}