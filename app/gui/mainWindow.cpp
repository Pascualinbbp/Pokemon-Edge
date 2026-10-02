#include "mainWindow.hpp"
#include "guiStyle.hpp"
#include "components/titleComponent.hpp"
#include "components/menuComponent.hpp"
#include "../utils/loggerUtil.hpp"
#include "../engine/gameEngine.hpp" // Motor 3D
#include "../engine/input.hpp"      // Input

#include <string>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static ID3D11DepthStencilView* g_depthStencilView = nullptr; 
static ID3D11ShaderResourceView* g_logoTexture = nullptr;
static int g_logoWidth = 0, g_logoHeight = 0;
static HWND g_hwnd = nullptr;

static GameEngine g_gameEngine;
static InputState g_inputState;

ID3D11Device* MainWindow::getDevice() { return g_pd3dDevice; }
ID3D11DeviceContext* MainWindow::getContext() { return g_pd3dDeviceContext; }

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    
    D3D11_TEXTURE2D_DESC descDepth = {};
    D3D11_TEXTURE2D_DESC bbDesc;
    pBackBuffer->GetDesc(&bbDesc);
    pBackBuffer->Release();

    descDepth.Width = bbDesc.Width;
    descDepth.Height = bbDesc.Height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    
    ID3D11Texture2D* pDepthStencil = nullptr;
    g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &pDepthStencil);
    g_pd3dDevice->CreateDepthStencilView(pDepthStencil, nullptr, &g_depthStencilView);
    pDepthStencil->Release();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) return true;
    switch (uMsg) {
        case WM_SIZE:
            if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
                if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
                if (g_depthStencilView) { g_depthStencilView->Release(); g_depthStencilView = nullptr; }
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_KEYDOWN:
            if (wParam == 'W') g_inputState.up = true;
            if (wParam == 'S') g_inputState.down = true;
            if (wParam == 'A') g_inputState.left = true;
            if (wParam == 'D') g_inputState.right = true;
            return 0;
        case WM_KEYUP:
            if (wParam == 'W') g_inputState.up = false;
            if (wParam == 'S') g_inputState.down = false;
            if (wParam == 'A') g_inputState.left = false;
            if (wParam == 'D') g_inputState.right = false;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void MainWindow::init() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, "PokemonEdgeEngine", nullptr };
    RegisterClassEx(&wc);
    g_hwnd = CreateWindowEx(0, wc.lpszClassName, "Pokemon Edge // Game Engine Studio", WS_OVERLAPPEDWINDOW, 100, 100, 960, 540, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 1, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pd3dDeviceContext);
    
    CreateRenderTarget();

    unsigned char* image_data = stbi_load("app/data/logo.png", &g_logoWidth, &g_logoHeight, nullptr, 4);
    if (image_data) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = g_logoWidth; desc.Height = g_logoHeight; desc.MipLevels = 1; desc.ArraySize = 1; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D* pTexture = nullptr;
        D3D11_SUBRESOURCE_DATA subResource = { image_data, static_cast<UINT>(desc.Width * 4), 0 };
        if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture))) {
            g_pd3dDevice->CreateShaderResourceView(pTexture, nullptr, &g_logoTexture);
            pTexture->Release();
        }
        stbi_image_free(image_data);
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    GuiStyle::applyTheme();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // FIX: Ahora solo le pasamos g_pd3dDevice (1 argumento)
    g_gameEngine.init(g_pd3dDevice);
}

void MainWindow::cleanup() {
    g_gameEngine.cleanup();
    if (g_logoTexture) { g_logoTexture->Release(); g_logoTexture = nullptr; }
    if (g_depthStencilView) { g_depthStencilView->Release(); g_depthStencilView = nullptr; }
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void MainWindow::run() {
    bool done = false;
    GameState currentState = GameState::TITLE_SCREEN;
    bool needsRedraw = true;

    LARGE_INTEGER freq, lastTime, currentTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastTime);

    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
            needsRedraw = true;
        }
        if (done) break;

        if (!needsRedraw && currentState != GameState::PLAYING) {
            WaitMessage();
            continue;
        }

        QueryPerformanceCounter(&currentTime);
        float dt = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) / freq.QuadPart;
        lastTime = currentTime;

        if (currentState == GameState::PLAYING) {
            g_gameEngine.update(dt, g_inputState);
        }

        const float clear_color[4] = { 0.4f, 0.6f, 0.9f, 1.00f }; 
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, g_depthStencilView);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        g_pd3dDeviceContext->ClearDepthStencilView(g_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

        if (currentState == GameState::PLAYING) {
            RECT rc;
            GetClientRect(g_hwnd, &rc);
            g_gameEngine.render(g_pd3dDeviceContext, rc.right - rc.left, rc.bottom - rc.top);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        GameState prevState = currentState;
        
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MainCanvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);

        switch (currentState) {
            case GameState::TITLE_SCREEN: TitleComponent::render(currentState, g_logoTexture, g_logoWidth, g_logoHeight); break;
            case GameState::MAIN_MENU: MenuComponent::render(currentState); break;
            case GameState::PLAYING:
                ImGui::SetCursorPos(ImVec2(10, 10));
                ImGui::TextColored(ImVec4(1,1,1,1), "Controles: WASD. FPS: %.1f", ImGui::GetIO().Framerate);
                if (ImGui::Button("SALIR AL MENU")) currentState = GameState::MAIN_MENU;
                break;
        }
        if (prevState != currentState) needsRedraw = true;

        ImGui::End();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        
        g_pSwapChain->Present(1, 0);
        needsRedraw = false;
    }
    cleanup();
}