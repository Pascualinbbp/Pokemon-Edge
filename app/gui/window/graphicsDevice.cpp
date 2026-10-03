#include "graphicsDevice.hpp"
#include "../../utils/core/fileUtil.hpp"
#include "../../utils/core/loggerUtil.hpp"
#include "../../utils/graphics/d3dUtil.hpp"
#include "../../utils/graphics/imageUtil.hpp"

using Microsoft::WRL::ComPtr;

namespace {
    ComPtr<ID3D11Device> g_device;
    ComPtr<ID3D11DeviceContext> g_context;
    ComPtr<IDXGISwapChain> g_swapChain;
    ComPtr<ID3D11RenderTargetView> g_rtv;
    ComPtr<ID3D11DepthStencilView> g_dsv;
    int g_width = 0;
    int g_height = 0;

    void createRenderTargets() {
        ComPtr<ID3D11Texture2D> backBuffer;
        D3dUtil::check(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)), "SwapChain::GetBuffer");
        D3dUtil::check(g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_rtv), "CreateRenderTargetView");

        D3D11_TEXTURE2D_DESC bbDesc = {};
        backBuffer->GetDesc(&bbDesc);
        g_width = static_cast<int>(bbDesc.Width);
        g_height = static_cast<int>(bbDesc.Height);

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = bbDesc.Width;
        depthDesc.Height = bbDesc.Height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        ComPtr<ID3D11Texture2D> depthTexture;
        D3dUtil::check(g_device->CreateTexture2D(&depthDesc, nullptr, &depthTexture), "CreateTexture2D (depth)");
        D3dUtil::check(g_device->CreateDepthStencilView(depthTexture.Get(), nullptr, &g_dsv), "CreateDepthStencilView");
    }

    void releaseRenderTargets() {
        // Hay que desvincular los targets antes de ResizeBuffers.
        if (g_context) g_context->OMSetRenderTargets(0, nullptr, nullptr);
        g_rtv.Reset();
        g_dsv.Reset();
    }
}

namespace GraphicsDevice {

    void init(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        HRESULT hr = E_FAIL;
        // GPU primero; si falla, renderizador por software (WARP).
        for (D3D_DRIVER_TYPE type : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP }) {
            hr = D3D11CreateDeviceAndSwapChain(nullptr, type, nullptr, 0, levels, 1, D3D11_SDK_VERSION,
                &sd, &g_swapChain, &g_device, nullptr, &g_context);
            if (SUCCEEDED(hr)) break;
        }
        D3dUtil::check(hr, "D3D11CreateDeviceAndSwapChain");
        createRenderTargets();
    }

    void cleanup() {
        releaseRenderTargets();
        g_swapChain.Reset();
        g_context.Reset();
        g_device.Reset();
    }

    void resize(UINT width, UINT height) {
        if (!g_swapChain || width == 0 || height == 0) return;
        try {
            releaseRenderTargets();
            D3dUtil::check(g_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");
            createRenderTargets();
        } catch (...) {
            // Ya registrado por D3dUtil::check. No se debe propagar una excepción desde el WndProc.
        }
    }

    void beginFrame(const float clearColor[4]) {
        if (!g_rtv || !g_dsv) return;

        ID3D11RenderTargetView* rtv = g_rtv.Get();
        g_context->OMSetRenderTargets(1, &rtv, g_dsv.Get());
        g_context->ClearRenderTargetView(rtv, clearColor);
        g_context->ClearDepthStencilView(g_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(g_width), static_cast<float>(g_height), 0.0f, 1.0f };
        g_context->RSSetViewports(1, &viewport);
    }

    void present() {
        g_swapChain->Present(1, 0);
    }

    bool loadTexture(const std::filesystem::path& path, Texture& out) {
        const auto bytes = FileUtil::readBinary(path);
        if (!bytes) {
            Logger::logError("GRAPHICS", "No se pudo abrir la textura: " + path.string());
            return false;
        }

        const auto image = ImageUtil::decode(*bytes);
        if (!image) {
            Logger::logError("GRAPHICS", "No se pudo decodificar la textura: " + path.string());
            return false;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = image->width;
        desc.Height = image->height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        const D3D11_SUBRESOURCE_DATA data = { image->pixels.get(), static_cast<UINT>(image->width * 4), 0 };
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = g_device->CreateTexture2D(&desc, &data, &texture);
        if (SUCCEEDED(hr)) hr = g_device->CreateShaderResourceView(texture.Get(), nullptr, &out.srv);
        if (FAILED(hr)) {
            Logger::logError("GRAPHICS", "No se pudo crear la textura en GPU: " + path.string());
            return false;
        }

        out.width = image->width;
        out.height = image->height;
        return true;
    }

    ID3D11Device* device() { return g_device.Get(); }
    ID3D11DeviceContext* context() { return g_context.Get(); }
    int width() { return g_width; }
    int height() { return g_height; }

} // namespace GraphicsDevice