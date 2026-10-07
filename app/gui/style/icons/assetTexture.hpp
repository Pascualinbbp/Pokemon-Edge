#pragma once
#include <string>
#include <vector>
#include "../../window/graphicsDevice.hpp"
#include "../../../utils/core/resourceUtil.hpp"

// Carga una textura desde un fichero de assets/ incrustado en el exe. Devuelve false si no existe o no se pudo cargar.
namespace AssetTexture {
    inline bool load(const std::string& path, Texture& texture) {
        const ResourceUtil::Blob blob = ResourceUtil::loadAsset(path);
        if (!blob) return false;
        return GraphicsDevice::loadTexture(std::vector<unsigned char>(blob.data, blob.data + blob.size), path, texture);
    }
}
