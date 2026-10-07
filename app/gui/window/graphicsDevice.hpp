#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

struct Texture {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    int width = 0;
    int height = 0;
};

// Todo lo relacionado con el dispositivo D3D11, swap chain y render targets.
namespace GraphicsDevice {
    void init(HWND hwnd);
    void cleanup();
    void resize(UINT width, UINT height);
    
    void beginFrame(const float clearColor[4]); // bind RT + clear + viewport
    void present();
    
    bool loadTexture(const std::filesystem::path& path, Texture& out);
    bool loadTexture(const std::vector<unsigned char>& bytes, const std::string& name, Texture& out); // desde memoria; 'name' solo para el log
    
    ID3D11Device* device();
    ID3D11DeviceContext* context();
    int width();
    int height();
}
