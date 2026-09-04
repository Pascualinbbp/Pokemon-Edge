#pragma once
#include <windows.h>
#include <string>

class GuiManager {
public:
    static void init();
    static void run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void onSearch(HWND hwnd);
    static void onListAll(HWND hwnd);

    static HWND hEditId;
    static HWND hButtonSearch;
    static HWND hButtonList;
    static HWND hListOutput;
    
    static HBRUSH hDarkBackground;
    static HBRUSH hDarkPanel;
    static HBRUSH hDarkElement;
};