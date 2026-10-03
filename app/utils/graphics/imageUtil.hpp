#pragma once
#include <memory>
#include <optional>
#include <vector>

// Decodificación de imágenes (PNG, JPG...). La implementación vive en imageUtil.cpp.
namespace ImageUtil {
    struct PixelDeleter {
        void operator()(unsigned char* pixels) const;
    };

    // Imagen decodificada a RGBA de 8 bits por canal.
    struct Image {
        std::unique_ptr<unsigned char, PixelDeleter> pixels;
        int width = 0;
        int height = 0;
    };

    // Decodifica los bytes de un archivo de imagen. Devuelve nullopt si el formato no es válido.
    std::optional<Image> decode(const std::vector<unsigned char>& fileBytes);
}