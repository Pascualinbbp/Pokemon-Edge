#pragma once
#include <windows.h>

class GuiManager {
public:
    static void init();
    static void run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};