#include "guiManager.hpp"
#include "databaseManager.hpp"
#include "../utils/loggerUtil.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <vector>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

enum class GameState {
    TITLE_SCREEN,
    MAIN_MENU,
    TYPE_DATABASE_STUDIO
};

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;
static ID3D11ShaderResourceView* g_logoTexture = NULL;
static int g_logoWidth = 0;
static int g_logoHeight = 0;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
bool LoadTextureFromFile(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void GuiManager::init() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "PokemonEdgeEngineTexture", NULL };
    RegisterClassEx(&wc);
    
    HWND hwnd = CreateWindowEx(
        0, wc.lpszClassName, 
        "Pokemon Edge // Game Engine Studio", 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 
        100, 100, 960, 540, 
        NULL, NULL, wc.hInstance, NULL
    );

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return;
    }

    // Cargar la textura del logotipo desde app/data/logo.png
    bool ret = LoadTextureFromFile("app/data/logo.png", &g_logoTexture, &g_logoWidth, &g_logoHeight);
    if (!ret) {
        Logger::logError("GUI_MANAGER", "No se pudo cargar 'app/data/logo.png'. Se usara renderizado vectorial alternativo.");
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]       = ImVec4(0.04f, 0.04f, 0.07f, 1.00f);
    colors[ImGuiCol_Header]         = ImVec4(0.85f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_Button]         = ImVec4(0.80f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.95f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.65f, 0.10f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBg]        = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    
    Logger::logInfo("GUI_MANAGER", "Interfaz grafica y recursos visuales cargados.");
}

void GuiManager::run() {
    bool done = false;
    GameState gameState = GameState::TITLE_SCREEN;

    static char searchBuffer[128] = "";
    static std::vector<PokemonType> cachedList;
    static bool listLoaded = false;
    static std::string searchResultMsg = "Sistema listo para operaciones.";
    float blinkTimer = 0.0f;

    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("GameContainerWindow", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (gameState == GameState::TITLE_SCREEN) {
            blinkTimer += 0.03f;
            bool showPressText = ((int)(blinkTimer * 2.2f) % 2) == 0;
            ImVec2 windowSize = ImGui::GetWindowSize();

            // Renderizar Logotipo en Imagen si existe, o respaldo vectorial tipográfico
            ImGui::SetCursorPosY(windowSize.y * 0.15f);
            if (g_logoTexture) {
                float renderW = 320.0f;
                float renderH = (g_logoHeight > 0 && g_logoWidth > 0) ? (renderW * ((float)g_logoHeight / (float)g_logoWidth)) : 120.0f;
                ImGui::SetCursorPosX((windowSize.x - renderW) * 0.5f);
                ImGui::Image((void*)g_logoTexture, ImVec2(renderW, renderH));
            } else {
                std::string titleBig = "P O K E M O N   E D G E";
                float tWidth = ImGui::CalcTextSize(titleBig.c_str()).x;
                ImGui::SetCursorPosX((windowSize.x - tWidth) * 0.5f);
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "%s", titleBig.c_str());
            }

            ImGui::SetCursorPosY(windowSize.y * 0.65f);
            if (showPressText) {
                std::string pressText = "- PULSA PARA CONTINUAR -";
                float pressWidth = ImGui::CalcTextSize(pressText.c_str()).x;
                ImGui::SetCursorPosX((windowSize.x - pressWidth) * 0.5f);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.9f), "%s", pressText.c_str());
            }

            ImGui::Dummy(ImVec2(0.0f, 15.0f));
            float btnWidth = 260.0f;
            ImGui::SetCursorPosX((windowSize.x - btnWidth) * 0.5f);
            if (ImGui::Button("INICIAR AVENTURA", ImVec2(btnWidth, 48))) {
                gameState = GameState::MAIN_MENU;
            }
        }
        else if (gameState == GameState::MAIN_MENU) {
            ImVec2 windowSize = ImGui::GetWindowSize();
            
            ImGui::SetCursorPosY(windowSize.y * 0.2f);
            std::string menuTitle = "MENÚ PRINCIPAL";
            float mTitleWidth = ImGui::CalcTextSize(menuTitle.c_str()).x;
            ImGui::SetCursorPosX((windowSize.x - mTitleWidth) * 0.5f);
            ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.30f, 1.0f), "%s", menuTitle.c_str());
            
            ImGui::Dummy(ImVec2(0.0f, 40.0f));
            float menuBtnWidth = 340.0f;
            
            ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
            if (ImGui::Button("NUEVA PARTIDA [Estudio de Tipos BD]", ImVec2(menuBtnWidth, 48))) {
                gameState = GameState::TYPE_DATABASE_STUDIO;
            }

            ImGui::Dummy(ImVec2(0.0f, 15.0f));
            ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
            if (ImGui::Button("OPCIONES DEL SISTEMA", ImVec2(menuBtnWidth, 48))) {
                searchResultMsg = "Configuración no disponible en esta build.";
            }

            ImGui::Dummy(ImVec2(0.0f, 15.0f));
            ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
            if (ImGui::Button("VOLVER AL TÍTULO", ImVec2(menuBtnWidth, 48))) {
                gameState = GameState::TITLE_SCREEN;
            }
        }
        else if (gameState == GameState::TYPE_DATABASE_STUDIO) {
            if (ImGui::Button("< Menú Principal")) {
                gameState = GameState::MAIN_MENU;
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.6f, 1.0f), "  |  MODULO: CONSULTA DE TIPOS DE POKEMON");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            if (ImGui::BeginChild("LeftControlPanel", ImVec2(350, 0), true)) {
                ImGui::Text("Panel de Control");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                ImGui::Text("ID del Tipo:");
                ImGui::SetNextItemWidth(200);
                ImGui::InputText("##searchId", searchBuffer, IM_ARRAYSIZE(searchBuffer), ImGuiInputTextFlags_CharsDecimal);
                
                ImGui::Dummy(ImVec2(0.0f, 8.0f));
                if (ImGui::Button("Consultar ID", ImVec2(200, 32))) {
                    std::string input(searchBuffer);
                    if (!input.empty()) {
                        try {
                            int id = std::stoi(input);
                            PokemonType res = DatabaseManager::getTypeDao().findById(id);
                            if (res.id != -1) {
                                searchResultMsg = "Encontrado -> ID: " + std::to_string(res.id) + " | Tipo: " + res.name;
                            } else {
                                searchResultMsg = "Aviso: No existe el ID " + input;
                            }
                        } catch (...) {
                            searchResultMsg = "Error: Formato numérico incorrecto.";
                        }
                    } else {
                        searchResultMsg = "Aviso: Introduce un ID válido.";
                    }
                }

                ImGui::Dummy(ImVec2(0.0f, 20.0f));
                if (ImGui::Button("Cargar Todos los Tipos", ImVec2(300, 38))) {
                    cachedList = DatabaseManager::getTypeDao().findAll();
                    listLoaded = true;
                    searchResultMsg = "Cargados " + std::to_string(cachedList.size()) + " registros.";
                }

                ImGui::Dummy(ImVec2(0.0f, 25.0f));
                ImGui::TextWrapped("Consola:\n%s", searchResultMsg.c_str());
            }
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("RightOutputPanel", ImVec2(0, 0), true)) {
                ImGui::Text("Visor de Registros Indexados");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (listLoaded) {
                    if (ImGui::BeginTable("TypesGameTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 390))) {
                        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                        ImGui::TableSetupColumn("Nombre del Tipo", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();

                        for (const auto& t : cachedList) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%d", t.id);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%s", t.name.c_str());
                        }
                        ImGui::EndTable();
                    }
                } else {
                    ImGui::TextDisabled("Haga clic en 'Cargar Todos los Tipos' para desplegar los datos.");
                }
            }
            ImGui::EndChild();
        }

        ImGui::End();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.04f, 0.04f, 0.07f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    if (g_logoTexture) g_logoTexture->Release();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
}

bool LoadTextureFromFile(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height) {
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL) return false;

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D *pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    ZeroMemory(&subResource, sizeof(subResource));
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    *out_width = image_width;
    *out_height = image_height;
    stbi_image_free(image_data);
    return true;
}

LRESULT CALLBACK GuiManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg) {
        case WM_SIZE:
            if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags, featureLevelArray, 2,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}