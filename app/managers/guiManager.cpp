#include "guiManager.hpp"
#include "databaseManager.hpp"
#include "../utils/loggerUtil.hpp"
#include <vector>

HWND GuiManager::hEditId = NULL;
HWND GuiManager::hButtonSearch = NULL;
HWND GuiManager::hButtonList = NULL;
HWND GuiManager::hListOutput = NULL;

HBRUSH GuiManager::hDarkBackground = NULL;
HBRUSH GuiManager::hDarkPanel = NULL;
HBRUSH GuiManager::hDarkElement = NULL;

void GuiManager::init() {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const char* CLASS_NAME = "PokemonEdgeGameEngineUI";

    // Inicializar pinceles para la interfaz estilo Modo Oscuro / Launcher de Juego 3D
    hDarkBackground = CreateSolidBrush(RGB(18, 18, 18));     // Fondo principal casi negro (#121212)
    hDarkPanel = CreateSolidBrush(RGB(30, 30, 30));         // Paneles secundarios (#1e1e1e)
    hDarkElement = CreateSolidBrush(RGB(45, 45, 45));       // Elementos de entrada (#2d2d2d)

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = hDarkBackground;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // Ventana principal con diseño profesional tipo Launcher de Videojuego (Dimensiones limpias, fija o redimensionable)
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Pokemon Edge - 3D Engine Launcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 450,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        Logger::logError("GUI_MANAGER", "Error crítico al instanciar la ventana de la interfaz gráfica.");
        return;
    }

    // Creación de componentes con estilo visual moderno y limpio
    // Etiqueta de Título Principal del Juego
    HWND hTitle = CreateWindow("STATIC", "POKEMON EDGE - DATABASE ENGINE", WS_VISIBLE | WS_CHILD, 30, 20, 520, 30, hwnd, NULL, hInstance, NULL);
    
    // Controles de Búsqueda por ID
    CreateWindow("STATIC", "ID de Tipo:", WS_VISIBLE | WS_CHILD, 30, 75, 90, 25, hwnd, NULL, hInstance, NULL);
    hEditId = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 130, 73, 100, 26, hwnd, NULL, hInstance, NULL);
    hButtonSearch = CreateWindow("BUTTON", "BUSCAR", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 240, 72, 110, 28, hwnd, (HMENU)1, hInstance, NULL);
    
    // Botón de Listado General
    hButtonList = CreateWindow("BUTTON", "CARGAR TODOS LOS TIPOS", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 360, 72, 190, 28, hwnd, (HMENU)2, hInstance, NULL);

    // Consola / Listbox de Resultados Estilizada
    hListOutput = CreateWindow("LISTBOX", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT, 30, 120, 520, 260, hwnd, NULL, hInstance, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    Logger::logInfo("GUI_MANAGER", "Interfaz gráfica profesional inicializada con éxito.");
}

void GuiManager::run() {
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Limpieza de recursos GDI al salir
    if (hDarkBackground) DeleteObject(hDarkBackground);
    if (hDarkPanel) DeleteObject(hDarkPanel);
    if (hDarkElement) DeleteObject(hDarkElement);
}

LRESULT CALLBACK GuiManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(220, 220, 220)); // Texto gris claro elegante
            SetBkMode(hdcStatic, TRANSPARENT);
            return (INT_PTR)hDarkBackground;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(255, 255, 255));   // Texto blanco en cajas de texto
            SetBkColor(hdcEdit, RGB(45, 45, 45));
            return (INT_PTR)hDarkElement;
        }
        case WM_CTLCOLORLISTBOX: {
            HDC hdcList = (HDC)wParam;
            SetTextColor(hdcList, RGB(0, 255, 128));     // Texto verde cyberpunk/game para los registros de BD
            SetBkColor(hdcList, RGB(24, 24, 24));
            return (INT_PTR)hDarkPanel;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) {
                onSearch(hwnd);
            } else if (LOWORD(wParam) == 2) {
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
    
    SendMessage(hListOutput, LB_RESETCONTENT, 0, 0);

    if (input.empty()) {
        SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"[ADVERTENCIA] Por favor, introduce un ID numerico valido.");
        return;
    }

    try {
        int idSearch = std::stoi(input);
        PokemonType result = DatabaseManager::getTypeDao().findById(idSearch);
        
        if (result.id != -1) {
            std::string msg = " > [REGISTRO ENCONTRADO] ID: " + std::to_string(result.id) + " | Tipo: " + result.name;
            SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
        } else {
            std::string msg = " > [AVISO] No existe ningun tipo de Pokemon asociado al ID: " + std::to_string(idSearch);
            SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
        }
    } catch (...) {
        SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"[ERROR] Parametro de entrada corrompido o fuera de rango.");
    }
}

void GuiManager::onListAll(HWND hwnd) {
    SendMessage(hListOutput, LB_RESETCONTENT, 0, 0);
    std::vector<PokemonType> list = DatabaseManager::getTypeDao().findAll();
    
    SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"=========================================");
    SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"       LISTADO GENERAL DE TIPOS          ");
    SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"=========================================");
    
    for (const auto& t : list) {
        std::string msg = " [ID: " + std::to_string(t.id) + "] ---> " + t.name;
        SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)msg.c_str());
    }
    
    SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)"=========================================");
    std::string totalMsg = " Total de registros cargados: " + std::to_string(list.size());
    SendMessage(hListOutput, LB_ADDSTRING, 0, (LPARAM)totalMsg.c_str());
}