#include "mainWindow.hpp"
#include "graphicsDevice.hpp"
#include "guiInput.hpp"
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
#include "../../managers/databaseManager.hpp"
#include "../../managers/saveManager.hpp"
#include "../../managers/sessionManager.hpp"
#include "../../utils/core/pathsUtil.hpp"
#include "../../utils/core/resourceUtil.hpp"
#include "../../utils/core/timeUtil.hpp"
#include "../../utils/graphics/windowUtil.hpp"

#include <algorithm>
#include <cstdint>
#include <windows.h>
#include <dbt.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    constexpr const char* kClassName = "PokemonEdgeEngine";
    constexpr const char* kWindowTitle = "Pokemon Edge // Game Engine Studio";
    constexpr int kWindowWidth = 960;
    constexpr int kWindowHeight = 540;

    constexpr UINT_PTR kDeviceTimerId = 1;
    constexpr UINT kDeviceRescanDelayMs = 1000;  // segunda búsqueda de mandos tras un cambio de dispositivos
    constexpr DWORD kMenuPollMs = 50;            // lectura del mando en los menús (solo con un mando conectado)
    constexpr float kMaxFrameSeconds = 0.1f;     // una pausa larga no debe disparar la simulación
    constexpr float kAutosaveSeconds = 30.0f;    // intervalo del autoguardado (solo cuenta mientras se juega)
    constexpr float kFirstAutosaveSeconds = 10.0f; // primer autoguardado tras entrar a la partida
    constexpr float kSavedNoticeSeconds = 2.5f;  // cuánto se muestra el aviso de guardado en el HUD
    constexpr float kSavedNoticeFadeIn = 0.25f;  // aparición del aviso
    constexpr float kSavedNoticeFadeOut = 0.5f;  // desvanecido final del aviso

    HWND g_hwnd = nullptr;
    Texture g_logo;
    WindowUtil::Fullscreen g_fullscreen;
    GameState g_state = GameState::TITLE_SCREEN; // a nivel de módulo para poder pausar desde WM_KILLFOCUS
    float g_loadTimer = 0.0f;   // segundos desde que empezó la carga
    int g_selectedSlot = -1;    // ranura pendiente de confirmar en la pantalla de reemplazo
    bool g_resizing = false;    // el usuario está arrastrando el borde de la ventana
    bool g_focused = false;
    float g_autosaveTimer = 0.0f;  // segundos de juego desde el último guardado
    float g_savedNotice = 0.0f;    // segundos restantes del aviso de guardado en el HUD

    // Opacidad del aviso de guardado: aparece y se desvanece suavemente.
    float savingAlpha() {
        if (g_savedNotice <= 0.0f) return 0.0f;
        const float fadeIn = (kSavedNoticeSeconds - g_savedNotice) / kSavedNoticeFadeIn;
        const float fadeOut = g_savedNotice / kSavedNoticeFadeOut;
        return (std::clamp)((std::min)(fadeIn, fadeOut), 0.0f, 1.0f);
    }

    // Redimensiona el swap chain solo si el tamaño del área cliente cambió de verdad.
    void applyResize(HWND hwnd) {
        const WindowUtil::Size size = WindowUtil::clientSize(hwnd);
        if (size.width != GraphicsDevice::width() || size.height != GraphicsDevice::height()) {
            GraphicsDevice::resize(static_cast<UINT>(size.width), static_cast<UINT>(size.height));
        }
        InputHandler::relockCursor(hwnd);
    }

    // Arranca la pantalla de carga de la partida que acaba de prepararse.
    void beginLoading() {
        g_loadTimer = 0.0f;
        g_autosaveTimer = kAutosaveSeconds - kFirstAutosaveSeconds;
        g_savedNotice = 0.0f;
        g_state = GameState::LOADING;
    }

    void startNewGame(int slot) {
        SessionManager::startNew(slot);
        beginLoading();
    }

    void startSavedGame(int slot) {
        SessionManager::startSaved(slot);
        beginLoading();
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
            case WM_DEVICECHANGE: // conexión o desconexión de mandos (aviso del sistema, sin sondeo)
                if (wParam == DBT_DEVNODES_CHANGED) {
                    InputHandler::onDeviceChange();
                    SetTimer(hwnd, kDeviceTimerId, kDeviceRescanDelayMs, nullptr); // el driver puede tardar en estar listo
                }
                return TRUE;
            case WM_TIMER:
                if (wParam == kDeviceTimerId) {
                    KillTimer(hwnd, kDeviceTimerId);
                    InputHandler::onDeviceChange();
                }
                return 0;
            case WM_MOUSEMOVE:
                InputHandler::onMouseMove(lParam);
                return 0;
            case WM_LBUTTONDOWN:
                InputHandler::onMouseButton(false, true);
                return 0;
            case WM_LBUTTONUP:
                InputHandler::onMouseButton(false, false);
                return 0;
            case WM_RBUTTONDOWN:
                InputHandler::onMouseButton(true, true);
                return 0;
            case WM_RBUTTONUP:
                InputHandler::onMouseButton(true, false);
                return 0;
            case WM_MOUSEWHEEL:
                InputHandler::onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
                return 0;
            case WM_INPUT:
                InputHandler::onRawInput(lParam);
                return DefWindowProc(hwnd, msg, wParam, lParam); // Raw Input exige llamar a DefWindowProc
            case WM_KEYDOWN:
                InputHandler::onKey(wParam, lParam, true);
                return 0;
            case WM_KEYUP:
                InputHandler::onKey(wParam, lParam, false);
                return 0;
            case WM_SETFOCUS:
                g_focused = true;
                return 0;
            case WM_KILLFOCUS:
                g_focused = false;
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
    g_hwnd = WindowUtil::create(kClassName, kWindowTitle, WindowProc, kWindowWidth, kWindowHeight);
    WindowUtil::setIcon(g_hwnd, ResourceUtil::loadIcon(IDI_ICON1));

    GraphicsDevice::init(g_hwnd);
    GraphicsDevice::loadTexture(PathsUtil::LOGO_PATH, g_logo); // el logo es opcional: si falla solo se registra
    InputHandler::init(g_hwnd);

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                                   // sin imgui.ini en disco
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // el mando navega los menús
    GuiStyle::applyTheme();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(GraphicsDevice::device(), GraphicsDevice::context());

    SessionManager::engine().init(GraphicsDevice::device());
    SessionManager::engine().setData(DatabaseManager::loadGameData());
}

void MainWindow::cleanup() {
    InputHandler::setMouseCapture(g_hwnd, false); // devuelve el cursor al sistema
    SessionManager::engine().cleanup();
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
    GameEngine& engine = SessionManager::engine();
    TimeUtil::FrameTimer timer;
    int redrawFrames = 2; // frames pendientes de dibujar cuando no hay animación continua
    bool wasPlaying = false;
    InputDevice shownDevice = InputHandler::activeDevice();

    const ImTextureID logoId = (ImTextureID)(intptr_t)g_logo.srv.Get();

    for (bool done = false; !done;) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            if (msg.message == WM_QUIT) done = true;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message != WM_INPUT) redrawFrames = 2; // los informes del mando no obligan a redibujar
        }
        if (done) break;

        InputHandler::refreshDevices();

        // Al entrar/salir del juego cambia quién lee el mando: el juego (InputHandler) o la interfaz (GuiInput).
        const bool playing = g_state == GameState::PLAYING;
        if (playing != wasPlaying) {
            if (playing) GuiInput::release();
            else GuiInput::suppress();
            wasPlaying = playing;
        }
        InputHandler::setMouseCapture(g_hwnd, playing);

        // En los menús el mando también maneja la interfaz: solo se redibuja si algo cambió.
        if (!playing && g_focused && InputHandler::hasGamepad() && GuiInput::pollChanged()) redrawFrames = 2;

        // El dispositivo activo cambió (mando conectado o desconectado, o cambio de uso): redibujar las ayudas.
        if (const InputDevice device = InputHandler::activeDevice(); device != shownDevice) {
            shownDevice = device;
            redrawFrames = 2;
        }

        if (redrawFrames == 0 && !isAnimated(g_state)) {
            // Espera pasiva. Sin mando (o sin foco) no se despierta nunca por iniciativa propia.
            // Con un mando conectado se despierta cada kMenuPollMs para leerlo: Windows no avisa de
            // las pulsaciones de los mandos Xbox (XInput).
            WindowUtil::waitForMessages((g_focused && InputHandler::hasGamepad()) ? kMenuPollMs : INFINITE);
            timer.reset(); // el tiempo dormido no cuenta como dt
            continue;
        }

        const float dt = timer.tick(kMaxFrameSeconds);

        bool justPaused = false;
        if (g_state == GameState::PLAYING) {
            const InputState input = InputHandler::poll(dt);
            if (input.pause || engine.update(dt, input)) {
                g_state = GameState::PAUSED;
                redrawFrames = 2;
                justPaused = true;
            } else {
                g_savedNotice = (std::max)(0.0f, g_savedNotice - dt);
                g_autosaveTimer += dt;
                if (g_autosaveTimer >= kAutosaveSeconds) {
                    g_autosaveTimer = 0.0f;
                    if (SessionManager::saveCurrent()) g_savedNotice = kSavedNoticeSeconds;
                }
            }
        } else if (g_state == GameState::LOADING) {
            g_loadTimer += dt;
        }

        // GUI: fondo de la GUI. Juego: fondo propio del motor.
        const bool inGame = isInGame(g_state);
        GraphicsDevice::beginFrame(inGame ? GameEngine::CLEAR_COLOR : GuiStyle::BACKGROUND);
        if (inGame) engine.render(GraphicsDevice::context(), GraphicsDevice::width(), GraphicsDevice::height());

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (g_state != GameState::PLAYING) GuiInput::feed();
        ImGui::NewFrame();
        // El ESC que acaba de pausar el juego no debe llegar también al menú de pausa (la cerraría al instante).
        if (justPaused) ImGui::GetIO().ClearInputKeys();

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
                        else g_state = GameState::REPLACE_MENU; // todas ocupadas: hay que eliminar una
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
                HudComponent::render(InputHandler::activeDevice(), engine.status(), savingAlpha());
                break;
            case GameState::PAUSED:
                PauseComponent::render(g_state);
                break;
            case GameState::CONTROLS:
                ControlsComponent::render(g_state, InputHandler::activeDevice());
                break;
        }
        ImGui::End();
        GuiInput::endFrame();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        GraphicsDevice::present();

        if (g_state != prevState) {
            redrawFrames = 2;
            g_fullscreen.set(g_hwnd, isFullscreen(g_state));
            if (isInGame(prevState) && !isInGame(g_state)) SessionManager::saveCurrent(); // salir al menú guarda la partida
        } else if (redrawFrames > 0) {
            --redrawFrames;
        }
    }
    if (isInGame(g_state)) SessionManager::saveCurrent(); // cerrar el juego en plena partida también guarda
    cleanup();
}
