#pragma once
#include <cstdint>
#include <map>
#include <utility>
#include <string>
#include "imgui.h"
#include "assetTexture.hpp"

// Imágenes del HUD y de los objetos (assets/sprites/): un clima es weather/<id>.png, un objeto items/<categoría>/<id>.png (el mismo nombre que su modelo 3D), una emisora radio/<id>.png, y ui/ el disco de día y noche.
// Se cargan la primera vez que se piden. Una imagen que no existe devuelve 0 y quien dibuja usa un dibujo de reserva.
namespace HudIcons {
    namespace detail {
        inline ImTextureID texture(const std::string& path) {
            static std::map<std::string, Texture> cache;
            auto it = cache.find(path);
            if (it == cache.end()) {
                Texture loaded;
                AssetTexture::load(path, loaded);
                it = cache.emplace(path, std::move(loaded)).first;
            }
            return (ImTextureID)(intptr_t)it->second.srv.Get();
        }
    }

    inline ImTextureID texture(const std::string& path) { return detail::texture(path); } // cualquier imagen de assets/ por su ruta (p. ej. sprites/radio/3.png)
    inline ImTextureID weather(int weatherId) { return detail::texture("sprites/weather/" + std::to_string(weatherId) + ".png"); }
    // Sprite de un objeto: assets/sprites/<modelo>.png, con el mismo nombre que su modelo 3D (assets/models/<modelo>.glb), p. ej. "items/pokeballs/1".
    inline ImTextureID sprite(const std::string& model) { return detail::texture("sprites/" + model + ".png"); }
    inline ImTextureID dayNight() { return detail::texture("sprites/ui/daynight.png"); }
}
