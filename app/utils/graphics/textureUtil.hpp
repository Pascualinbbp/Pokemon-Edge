#pragma once
#include <algorithm>
#include <vector>
#include "d3dUtil.hpp"
#include "imageUtil.hpp"

// Texturas de GPU a partir de imágenes ya decodificadas.
namespace TextureUtil {
    // Textura RGBA de 8 bits, inmutable, con todos sus mipmaps (reducción por promedio de 2x2).
    inline D3dUtil::ComPtr<ID3D11ShaderResourceView> createFromImage(ID3D11Device* device, const ImageUtil::Image& image) {
        std::vector<std::vector<unsigned char>> levels;
        std::vector<int> widths, heights;
        levels.emplace_back(image.pixels.get(), image.pixels.get() + static_cast<size_t>(image.width) * image.height * 4);
        widths.push_back(image.width);
        heights.push_back(image.height);
        while (widths.back() > 1 || heights.back() > 1) {
            const int w = widths.back(), h = heights.back();
            const int nw = (std::max)(1, w / 2), nh = (std::max)(1, h / 2);
            std::vector<unsigned char> next(static_cast<size_t>(nw) * nh * 4);
            const std::vector<unsigned char>& src = levels.back();
            for (int y = 0; y < nh; ++y) {
                for (int x = 0; x < nw; ++x) {
                    const int x0 = (std::min)(x * 2, w - 1), x1 = (std::min)(x * 2 + 1, w - 1);
                    const int y0 = (std::min)(y * 2, h - 1), y1 = (std::min)(y * 2 + 1, h - 1);
                    for (int c = 0; c < 4; ++c) {
                        const int sum = src[(static_cast<size_t>(y0) * w + x0) * 4 + c] + src[(static_cast<size_t>(y0) * w + x1) * 4 + c] +
                                        src[(static_cast<size_t>(y1) * w + x0) * 4 + c] + src[(static_cast<size_t>(y1) * w + x1) * 4 + c];
                        next[(static_cast<size_t>(y) * nw + x) * 4 + c] = static_cast<unsigned char>((sum + 2) / 4);
                    }
                }
            }
            levels.push_back(std::move(next));
            widths.push_back(nw);
            heights.push_back(nh);
        }

        std::vector<D3D11_SUBRESOURCE_DATA> data(levels.size());
        for (size_t i = 0; i < levels.size(); ++i) {
            data[i].pSysMem = levels[i].data();
            data[i].SysMemPitch = static_cast<UINT>(widths[i] * 4);
        }
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = static_cast<UINT>(image.width);
        desc.Height = static_cast<UINT>(image.height);
        desc.MipLevels = static_cast<UINT>(levels.size());
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3dUtil::ComPtr<ID3D11Texture2D> texture;
        D3dUtil::check(device->CreateTexture2D(&desc, data.data(), &texture), "CreateTexture2D (model)");
        D3dUtil::ComPtr<ID3D11ShaderResourceView> view;
        D3dUtil::check(device->CreateShaderResourceView(texture.Get(), nullptr, &view), "CreateShaderResourceView (model)");
        return view;
    }
}
