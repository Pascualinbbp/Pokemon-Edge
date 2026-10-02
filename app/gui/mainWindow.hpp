#pragma once
#include <windows.h>
#include <d3d11.h>

enum class GameState {
    TITLE_SCREEN,
    MAIN_MENU,
    PLAYING
};

namespace MainWindow {
    void init();
    void run();
    void cleanup();

    // Exponemos el device para el motor 3D
    ID3D11Device* getDevice();
    ID3D11DeviceContext* getContext();
}