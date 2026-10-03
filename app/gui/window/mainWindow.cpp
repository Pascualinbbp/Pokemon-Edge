#include "mainWindow.hpp"
#include "graphicsDevice.hpp"
#include "inputHandler.hpp"
#include "../gameState.hpp"
#include "../style/guiStyle.hpp"
#include "../components/titleComponent.hpp"
#include "../components/menuComponent.hpp"
#include "../components/hudComponent.hpp"
#include "../components/pauseComponent.hpp"
#include "../components/controlsComponent.hpp"
#include "../../utils/core/pathsUtil.hpp"
#include "../../engine/core/gameEngine.hpp"

#include <algorithm>
#include <cstdint>
#include <windows.h>
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

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;

        switch (msg) {
            case WM_SIZE:
                if (wParam != SIZE_MINIMIZED) {
                    GraphicsDevice::resize(LOWORD(lParam), HIWORD(lParam));
                    InputHandler::relockCursor(hwnd);
                }
                return 0;
            case WM_MOVE:
                InputHandler::relockCursor(hwnd);
                return 0;
            case WM_INPUT:
                InputHandler::onRawInput(lParam);
                return DefWindowProc(hwnd, msg, wParam, lParam); // Raw Input exige llamar a DefWindowProc
            case WM_KEYDOWN:
                InputHandler::onKey(wParam, true);
                return 0;
            case WM_KEYUP:
                InputHandler::onKey(wParam, false);
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

        // El ratón solo se captura mientras se juega.
        InputHandler::setMouseCapture(g_hwnd, g_state == GameState::PLAYING);

        // Título (parpadeo) y juego se animan siempre; los menús solo se redibujan con eventos.
        const bool animated = g_state == GameState::TITLE_SCREEN || g_state == GameState::PLAYING;
        if (redrawFrames == 0 && !animated) {
            WaitMessage();
            QueryPerformanceCounter(&last); // el tiempo dormido no cuenta como dt
            continue;
        }

        QueryPerformanceCounter(&now);
        const float dt = (std::min)(static_cast<float>(now.QuadPart - last.QuadPart) / freq.QuadPart, 0.1f);
        last = now;

        if (g_state == GameState::PLAYING) g_gameEngine.update(dt, InputHandler::poll());

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
            case GameState::TITLE_SCREEN: TitleComponent::render(g_state, logoId, g_logo.width, g_logo.height); break;
            case GameState::MAIN_MENU:    MenuComponent::render(g_state); break;
            case GameState::PLAYING:      HudComponent::render(g_state); break;
            case GameState::PAUSED:       PauseComponent::render(g_state); break;
            case GameState::CONTROLS:     ControlsComponent::render(g_state); break;
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        GraphicsDevice::present();

        if (g_state != prevState) redrawFrames = 2;
        else if (redrawFrames > 0) --redrawFrames;
    }
    cleanup();
}