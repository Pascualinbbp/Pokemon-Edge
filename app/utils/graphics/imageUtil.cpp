#include "imageUtil.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace ImageUtil {
    void PixelDeleter::operator()(unsigned char* pixels) const {
        stbi_image_free(pixels);
    }

    std::optional<Image> decode(const std::vector<unsigned char>& fileBytes) {
        int width = 0, height = 0;
        unsigned char* pixels = stbi_load_from_memory(fileBytes.data(), static_cast<int>(fileBytes.size()),
            &width, &height, nullptr, 4);
        if (!pixels) return std::nullopt;

        Image image;
        image.pixels.reset(pixels);
        image.width = width;
        image.height = height;
        return std::optional<Image>(std::move(image));
    }
}