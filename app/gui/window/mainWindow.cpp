#include "mainWindow.hpp"
#include "graphicsDevice.hpp"
#include "inputHandler.hpp"
#include "../gameState.hpp"
#include "../style/guiStyle.hpp"
#include "../components/titleComponent.hpp"
#include "../components/menuComponent.hpp"
#include "../components/slotsComponent.hpp"
#include "../components/loadingComponent.hpp"
#include "../components/hudComponent.hpp"
#include "../components/pauseComponent.hpp"
#include "../components/controlsComponent.hpp"
#include "../../managers/saveManager.hpp"
#include "../../utils/core/pathsUtil.hpp"
#include "../../engine/core/gameEngine.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <windows.h>
#include <dbt.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    constexpr const char* kClassName = "PokemonEdgeEngine";
    constexpr const char* kWindowTitle = "Pokemon Edge // Game Engine Studio";

    HWND g_hwnd = nullptr;
    Texture g_logo;
    GameEngine g_gameEngine;
    GameState g_state = GameState::TITLE_SCREEN; // a nivel de módulo para poder pausar desde WM_KILLFOCUS
    WINDOWPLACEMENT g_windowedPlacement = { sizeof(WINDOWPLACEMENT) };
    float g_loadTimer = 0.0f;   // segundos desde que empezó la carga
    int g_activeSlot = -1;      // ranura de la partida en curso
    int g_selectedSlot = -1;    // ranura pendiente de confirmar en la pantalla de reemplazo
    bool g_resizing = false;    // el usuario está arrastrando el borde de la ventana
    bool g_fullscreen = false;
    bool g_saved = false;       // mostrar el aviso de "partida guardada" en la pausa

    // Redimensiona el swap chain solo si el tamaño del área cliente cambió de verdad.
    void applyResize(HWND hwnd) {
        RECT client;
        GetClientRect(hwnd, &client);
        const int width = client.right;
        const int height = client.bottom;
        if (width != GraphicsDevice::width() || height != GraphicsDevice::height()) {
            GraphicsDevice::resize(static_cast<UINT>(width), static_cast<UINT>(height));
        }
        InputHandler::relockCursor(hwnd);
    }

    // Pantalla completa sin bordes; al salir se restaura la posición y el tamaño anteriores.
    void setFullscreen(bool enable) {
        if (enable == g_fullscreen) return;
        g_fullscreen = enable;

        if (enable) {
            MONITORINFO monitor = { sizeof(monitor) };
            GetWindowPlacement(g_hwnd, &g_windowedPlacement);
            GetMonitorInfo(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
            SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(g_hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        } else {
            SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowPlacement(g_hwnd, &g_windowedPlacement);
            SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }

    // Arranca la pantalla de carga de la partida de una ranura.
    void beginLoading(int slot) {
        g_activeSlot = slot;
        g_loadTimer = 0.0f;
        g_saved = false;
        g_state = GameState::LOADING;
    }

    // Partida nueva: ocupa la ranura desde el primer momento.
    void startNewGame(int slot) {
        g_gameEngine.newGame();
        SaveManager::save(slot, g_gameEngine.captureSave());
        beginLoading(slot);
    }

    void startSavedGame(int slot) {
        if (const std::optional<SaveData> save = SaveManager::load(slot)) g_gameEngine.applySave(*save);
        else g_gameEngine.newGame(); // partida dañada: se empieza de cero en esa ranura
        beginLoading(slot);
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;

        switch (msg) {
            case WM_ENTERSIZEMOVE:
                g_resizing = true;
                return 0;
            case WM_EXITSIZEMOVE: // el swap chain se redimensiona una sola vez, al soltar el borde
                g_resizing = false;
                applyResize(hwnd);
                return 0;
            case WM_SIZE: // maximizar/restaurar/pantalla completa llegan fuera del arrastre
                if (wParam != SIZE_MINIMIZED && !g_resizing) applyResize(hwnd);
                return 0;
            case WM_MOVE:
                InputHandler::relockCursor(hwnd);
                return 0;
            case WM_DEVICECHANGE: // conexión o desconexión de mandos
                if (wParam == DBT_DEVNODES_CHANGED) InputHandler::onDeviceChange();
                return TRUE;
            case WM_INPUT:
                InputHandler::onRawInput(lParam);
                return DefWindowProc(hwnd, msg, wParam, lParam); // Raw Input exige llamar a DefWindowProc
            case WM_KEYDOWN:
                InputHandler::onKey(wParam, lParam, true);
                return 0;
            case WM_KEYUP:
                InputHandler::onKey(wParam, lParam, false);
                return 0;
            case WM_KILLFOCUS:
                InputHandler::resetKeys();
                if (g_state == GameState::PLAYING) g_state = GameState::PAUSED;
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void MainWindow::init() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(nullptr),
        nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, kClassName, nullptr };
    RegisterClassEx(&wc);
    g_hwnd = CreateWindowEx(0, kClassName, kWindowTitle, WS_OVERLAPPEDWINDOW, 100, 100, 960, 540,
        nullptr, nullptr, wc.hInstance, nullptr);

    HICON hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(102));
    if (hIcon) {
        SendMessage(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    GraphicsDevice::init(g_hwnd);
    GraphicsDevice::loadTexture(PathsUtil::LOGO_PATH, g_logo); // el logo es opcional: si falla solo se registra
    InputHandler::init();

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    GuiStyle::applyTheme();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(GraphicsDevice::device(), GraphicsDevice::context());

    g_gameEngine.init(GraphicsDevice::device());
}

void MainWindow::cleanup() {
    InputHandler::setMouseCapture(g_hwnd, false); // devuelve el cursor al sistema
    g_gameEngine.cleanup();
    g_logo.srv.Reset();

    // ImGui debe cerrarse ANTES de liberar el dispositivo D3D.
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    GraphicsDevice::cleanup();
}

void MainWindow::run() {
    int redrawFrames = 2; // frames pendientes de dibujar cuando no hay animación continua

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    const ImTextureID logoId = (ImTextureID)(intptr_t)g_logo.srv.Get();

    for (bool done = false; !done;) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            if (msg.message == WM_QUIT) done = true;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            redrawFrames = 2;
        }
        if (done) break;

        InputHandler::refreshDevices();

        // El ratón solo se captura mientras se juega.
        InputHandler::setMouseCapture(g_hwnd, g_state == GameState::PLAYING);

        if (redrawFrames == 0 && !isAnimated(g_state)) {
            WaitMessage();
            QueryPerformanceCounter(&last); // el tiempo dormido no cuenta como dt
            continue;
        }

        QueryPerformanceCounter(&now);
        const float dt = (std::min)(static_cast<float>(now.QuadPart - last.QuadPart) / freq.QuadPart, 0.1f);
        last = now;

        if (g_state == GameState::PLAYING) g_gameEngine.update(dt, InputHandler::poll(dt));
        else if (g_state == GameState::LOADING) g_loadTimer += dt;

        // GUI: fondo de la GUI. Juego: fondo propio del motor.
        const bool inGame = isInGame(g_state);
        GraphicsDevice::beginFrame(inGame ? GameEngine::CLEAR_COLOR : GuiStyle::BACKGROUND);
        if (inGame) {
            g_gameEngine.render(GraphicsDevice::context(), GraphicsDevice::width(), GraphicsDevice::height());
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const GameState prevState = g_state;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MainCanvas", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
        switch (g_state) {
            case GameState::TITLE_SCREEN:
                TitleComponent::render(g_state, logoId, g_logo.width, g_logo.height);
                break;
            case GameState::MAIN_MENU:
                switch (MenuComponent::render(g_state, SaveManager::hasSaves())) {
                    case MenuComponent::Action::NEW_GAME: {
                        const int slot = SaveManager::freeSlot();
                        if (slot >= 0) startNewGame(slot);
                        else g_state = GameState::REPLACE_MENU; // 4 partidas: hay que eliminar una
                        break;
                    }
                    case MenuComponent::Action::LOAD_GAME:
                        g_state = GameState::LOAD_MENU;
                        break;
                    case MenuComponent::Action::NONE:
                        break;
                }
                break;
            case GameState::LOAD_MENU: {
                const int slot = SlotsComponent::renderLoad(g_state);
                if (slot >= 0) startSavedGame(slot);
                break;
            }
            case GameState::REPLACE_MENU: {
                const int slot = SlotsComponent::renderReplace(g_state, g_selectedSlot);
                if (slot >= 0) {
                    SaveManager::remove(slot);
                    startNewGame(slot);
                }
                break;
            }
            case GameState::LOADING:
                LoadingComponent::render(g_state, g_loadTimer);
                break;
            case GameState::PLAYING:
                HudComponent::render(g_state);
                break;
            case GameState::PAUSED:
                if (PauseComponent::render(g_state, g_saved)) {
                    g_saved = SaveManager::save(g_activeSlot, g_gameEngine.captureSave());
                }
                break;
            case GameState::CONTROLS:
                ControlsComponent::render(g_state, InputHandler::activeDevice());
                break;
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        GraphicsDevice::present();

        if (g_state != prevState) {
            redrawFrames = 2;
            setFullscreen(isFullscreen(g_state));
            if (g_state == GameState::PLAYING) g_saved = false;
        } else if (redrawFrames > 0) {
            --redrawFrames;
        }
    }
    cleanup();
}