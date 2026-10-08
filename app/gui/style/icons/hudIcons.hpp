#pragma once
#include <cctype>
#include <cstdint>
#include <map>
#include <utility>
#include <string>
#include "imgui.h"
#include "assetTexture.hpp"

// Logos del HUD (assets/hud/): uno por clima (weather/<nombre>.png) y por hábitat (habitat/<nombre>.png), donde el nombre
// es el de la base de datos en minúsculas, sin tildes y con '_' en vez de espacios; y el disco de día y noche (daynight.png).
// Se cargan la primera vez que se piden. Un nombre sin logo devuelve 0 y quien dibuja usa un círculo de reserva.
namespace HudIcons {
    namespace detail {
        inline std::string slug(const std::string& name) {
            static const std::pair<const char*, char> ACCENTS[] = { { "á", 'a' }, { "é", 'e' }, { "í", 'i' }, { "ó", 'o' }, { "ú", 'u' }, { "ñ", 'n' }, { "ü", 'u' } };
            std::string result;
            for (size_t i = 0; i < name.size();) {
                bool replaced = false;
                for (const auto& accent : ACCENTS) {
                    const size_t length = std::char_traits<char>::length(accent.first);
                    if (name.compare(i, length, accent.first) == 0) {
                        result += accent.second;
                        i += length;
                        replaced = true;
                        break;
                    }
                }
                if (replaced) continue;
                const unsigned char c = static_cast<unsigned char>(name[i++]);
                result += c == ' ' ? '_' : static_cast<char>(std::tolower(c));
            }
            return result;
        }

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

    inline ImTextureID weather(const std::string& name) { return detail::texture("hud/weather/" + detail::slug(name) + ".png"); }
    inline ImTextureID habitat(const std::string& name) { return detail::texture("hud/habitat/" + detail::slug(name) + ".png"); }
    inline ImTextureID dayNight() { return detail::texture("hud/daynight.png"); }
}
