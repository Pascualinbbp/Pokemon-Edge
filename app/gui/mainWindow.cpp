#include "mainWindow.hpp"
#include "guiStyle.hpp"
#include "components/titleComponent.hpp"
#include "components/menuComponent.hpp"
#include "../utils/loggerUtil.hpp"
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static ID3D11ShaderResourceView* g_logoTexture = nullptr;
static int g_logoWidth = 0, g_logoHeight = 0;
static HWND g_hwnd = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) return true;
    switch (uMsg) {
        case WM_SIZE:
            if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
                if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D* pBackBuffer = nullptr;
                if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
                    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
                    pBackBuffer->Release();
                }
            }
            return 0;
        case WM_DESTROY:
            Logger::logInfo("MAIN_WINDOW", "Recibido evento WM_DESTROY. Cerrando la aplicación.");
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void MainWindow::init() {
    Logger::logInfo("MAIN_WINDOW", "== INICIO DE MainWindow::init() ==");
    Logger::logInfo("MAIN_WINDOW", "Registrando la clase de ventana de Windows (WNDCLASSEX)...");
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, "PokemonEdgeEngine", nullptr };
    
    if (!RegisterClassEx(&wc)) {
        Logger::logError("MAIN_WINDOW", "Fallo crítico: No se pudo registrar la clase de ventana. Código de error WIN32: " + std::to_string(GetLastError()));
        return;
    }
    
    Logger::logInfo("MAIN_WINDOW", "Llamando a CreateWindowEx para generar la GUI...");
    g_hwnd = CreateWindowEx(0, wc.lpszClassName, "Pokemon Edge // Game Engine Studio", WS_OVERLAPPEDWINDOW, 100, 100, 960, 540, nullptr, nullptr, wc.hInstance, nullptr);

    if (!g_hwnd) {
        Logger::logError("MAIN_WINDOW", "Fallo crítico: No se pudo crear la ventana principal. Código de error WIN32: " + std::to_string(GetLastError()));
        return;
    }
    Logger::logInfo("MAIN_WINDOW", "Ventana de Windows creada exitosamente.");

    Logger::logInfo("MAIN_WINDOW", "Preparando inicialización de DirectX 11...");
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    
    Logger::logInfo("MAIN_WINDOW", "Llamando a D3D11CreateDeviceAndSwapChain...");
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pd3dDeviceContext);
    if (hr != S_OK) {
        Logger::logError("MAIN_WINDOW", "Fallo crítico: D3D11CreateDeviceAndSwapChain falló con HRESULT: " + std::to_string(hr));
        return;
    }
    Logger::logInfo("MAIN_WINDOW", "Dispositivo de DirectX 11 y SwapChain inicializados correctamente.");

    Logger::logInfo("MAIN_WINDOW", "Obteniendo BackBuffer del SwapChain...");
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr)) {
        Logger::logError("MAIN_WINDOW", "Fallo crítico: No se pudo obtener el BackBuffer. HRESULT: " + std::to_string(hr));
        return;
    }

    Logger::logInfo("MAIN_WINDOW", "Creando RenderTargetView...");
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) {
        Logger::logError("MAIN_WINDOW", "Fallo crítico: No se pudo crear el RenderTargetView. HRESULT: " + std::to_string(hr));
        return;
    }
    Logger::logInfo("MAIN_WINDOW", "RenderTargetView creado exitosamente.");

    Logger::logInfo("MAIN_WINDOW", "Intentando cargar app/data/logo.png desde el disco...");
    unsigned char* image_data = stbi_load("app/data/logo.png", &g_logoWidth, &g_logoHeight, nullptr, 4);
    if (image_data) {
        Logger::logInfo("MAIN_WINDOW", "Logo cargado en memoria. Creando textura en DX11...");
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = g_logoWidth; desc.Height = g_logoHeight; desc.MipLevels = 1; desc.ArraySize = 1; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D* pTexture = nullptr;
        D3D11_SUBRESOURCE_DATA subResource = { image_data, static_cast<UINT>(desc.Width * 4), 0 };
        hr = g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);
        if (SUCCEEDED(hr)) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; 
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; 
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            hr = g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &g_logoTexture);
            if (FAILED(hr)) {
                Logger::logError("MAIN_WINDOW", "Fallo al crear ShaderResourceView para el logo. HRESULT: " + std::to_string(hr));
            } else {
                Logger::logInfo("MAIN_WINDOW", "Textura DX11 del logo creada con éxito.");
            }
            pTexture->Release();
        } else {
            Logger::logError("MAIN_WINDOW", "Fallo al crear la textura 2D en DirectX 11 para el logo. HRESULT: " + std::to_string(hr));
        }
        stbi_image_free(image_data);
    } else {
        Logger::logError("MAIN_WINDOW", "No se pudo encontrar/cargar app/data/logo.png. Se omitirá el logo.");
    }

    Logger::logInfo("MAIN_WINDOW", "Ejecutando ShowWindow y UpdateWindow...");
    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    Logger::logInfo("MAIN_WINDOW", "Inicializando contexto de ImGui...");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    GuiStyle::applyTheme();
    
    Logger::logInfo("MAIN_WINDOW", "Inicializando impl de ImGui Win32...");
    if (!ImGui_ImplWin32_Init(g_hwnd)) {
        Logger::logError("MAIN_WINDOW", "ImGui_ImplWin32_Init devolvió false.");
    }

    Logger::logInfo("MAIN_WINDOW", "Inicializando impl de ImGui DX11...");
    if (!ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext)) {
        Logger::logError("MAIN_WINDOW", "ImGui_ImplDX11_Init devolvió false.");
    }
    
    Logger::logInfo("MAIN_WINDOW", "¡Función init() de MainWindow completada con éxito!");
}

void MainWindow::cleanup() {
    Logger::logInfo("MAIN_WINDOW", "Liberando recursos de la aplicación...");
    if (g_logoTexture) { g_logoTexture->Release(); g_logoTexture = nullptr; }
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    Logger::logInfo("MAIN_WINDOW", "Recursos liberados de forma segura.");
}

void MainWindow::run() {
    if (!g_hwnd || !g_pd3dDeviceContext || !g_mainRenderTargetView) {
        Logger::logError("MAIN_WINDOW", "El bucle run() se abortó prematuramente debido a componentes nulos (¿falló init()?).");
        return;
    }

    Logger::logInfo("MAIN_WINDOW", "Iniciando bucle de renderizado optimizado por eventos...");
    bool done = false;
    GameState currentState = GameState::TITLE_SCREEN;
    bool needsRedraw = true;

    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                Logger::logInfo("MAIN_WINDOW", "Mensaje de salida recibido, rompiendo bucle.");
                done = true;
            }
            needsRedraw = true; // Forzar redibujado solo cuando haya actividad de usuario o mensajes
        }
        if (done) break;

        // Si no hay cambios ni eventos, suspendemos inteligentemente el hilo para no quemar la CPU
        if (!needsRedraw) {
            WaitMessage();
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MainCanvas", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        GameState prevState = currentState;
        switch (currentState) {
            case GameState::TITLE_SCREEN:
                TitleComponent::render(currentState, g_logoTexture, g_logoWidth, g_logoHeight);
                break;
            case GameState::MAIN_MENU:
                MenuComponent::render(currentState);
                break;
        }

        if (prevState != currentState) {
            needsRedraw = true;
        }

        ImGui::End();
        ImGui::Render();
        
        const float clear_color[4] = { 0.04f, 0.04f, 0.07f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);

        needsRedraw = false; // Resetear hasta la siguiente interacción
    }
    
    cleanup();
}