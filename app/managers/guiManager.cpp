#include "guiManager.hpp"
#include "databaseManager.hpp"
#include "../utils/loggerUtil.hpp"
#include <vector>
#include <stdexcept>

HWND GuiManager::hEditId = NULL;
HWND GuiManager::hButtonSearch = NULL;
HWND GuiManager::hButtonList = NULL;
HWND GuiManager::hListOutput = NULL;

void GuiManager::init() {
    const char* CLASS_NAME = "PokemonEdgeUI";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Pokemon Edge - Motor 3D (GUI)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 420,
        NULL, NULL, wc.hInstance, NULL
    );

    if (hwnd == NULL) {
        Logger::logError("GUI_MANAGER", "Error al crear la ventana principal.");
        return;
    }

    // Crear controles básicos
    CreateWindow("STATIC", "ID del Tipo:", WS_VISIBLE | WS_CHILD, 20, 20, 80, 20, hwnd, NULL, NULL, NULL);
    hEditId = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 110, 20, 80, 20, hwnd, NULL, NULL, NULL);
    hButtonSearch = CreateWindow("BUTTON", "Buscar", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 200, 20, 80, 20, hwnd, (HMENU)1, NULL, NULL);
    hButtonList = CreateWindow("BUTTON", "Listar Todos", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 50, 120, 30, hwnd, (HMENU)2, NULL, NULL);
    hListOutput = CreateWindow("LISTBOX", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL, 20, 90, 360, 260, hwnd, NULL, NULL, NULL);

    ShowWindow(hwnd, SW_SHOW);
    Logger::logInfo("GUI_MANAGER", "Interfaz grafica iniciada correctamente.");
}

void GuiManager::run() {
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK GuiManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { // Botón Buscar
                onSearch(hwnd);
            } else if (LOWORD(wParam) == 2) { // Botón Listar
                onListAll(hwnd);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void GuiManager::onSearch(HWND hwnd) {
    char buffer[256];
    GetWindowText(hEditId, buffer, 256);
    std::string input(buffer);
    
    SendMessage(hListOutput, LB_RESETCONTENT, 0, 0); // Limpiar lista

    if (input.empty()) {
        SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"[Aviso] Ingrese un ID valido.");
        return;
    }

    try {
        int idSearch = std::stoi(input);
        PokemonType result = DatabaseManager::getTypeDao().findById(idSearch);
        
        if (result.id != -1) {
            std::string msg = "[Encontrado] ID: " + std::to_string(result.id) + " | Nombre: " + result.name;
            SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
        } else {
            std::string msg = "[Aviso] No se encontro ID " + std::to_string(idSearch);
            SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
        }
    } catch (...) {
        SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"[Error] Entrada invalida.");
    }
}

void GuiManager::onListAll(HWND hwnd) {
    SendMessage(hListOutput, LB_RESETCONTENT, 0, 0); // Limpiar lista
    std::vector<PokemonType> list = DatabaseManager::getTypeDao().findAll();
    
    SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"--- LISTA DE TIPOS DE POKEMON ---");
    for (const auto& t : list) {
        std::string msg = "ID: " + std::to_string(t.id) + " - " + t.name;
        SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
    }
}