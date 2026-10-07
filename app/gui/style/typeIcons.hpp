#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <type_traits>
#include "imgui.h"
#include "guiCards.hpp"
#include "../window/graphicsDevice.hpp"
#include "../../models/pokemonType.hpp"
#include "../../utils/core/pathsUtil.hpp"

// Icono de cada tipo: la imagen app/data/types/<id del tipo>.png (junto al logo). Se carga la primera vez que se pide;
// si no existe se dibuja un círculo con el color del tipo y su inicial.
namespace TypeIcons {
    namespace detail {
        inline std::map<int, Texture>& cache() {
            static std::map<int, Texture> textures;
            return textures;
        }

        inline ImTextureID texture(int typeId) {
            auto& textures = cache();
            auto it = textures.find(typeId);
            if (it == textures.end()) {
                Texture loaded;
                const std::filesystem::path file = std::filesystem::path(PathsUtil::LOGO_PATH).parent_path() / "types" / (std::to_string(typeId) + ".png");
                if (std::filesystem::exists(file)) {
                    using PathType = std::decay_t<decltype(PathsUtil::LOGO_PATH)>;
                    if constexpr (std::is_constructible_v<PathType, std::string>) GraphicsDevice::loadTexture(PathType(file.string()), loaded);
                    else GraphicsDevice::loadTexture(file.string().c_str(), loaded);
                }
                it = textures.emplace(typeId, std::move(loaded)).first;
            }
            return (ImTextureID)(intptr_t)it->second.srv.Get();
        }
    }

    // Icono circular del tipo centrado en 'c'.
    inline void draw(ImDrawList* dl, const ImVec2& c, float radius, const PokemonType& type) {
        if (const ImTextureID id = detail::texture(type.id)) {
            dl->AddImageRounded(id, ImVec2(c.x - radius, c.y - radius), ImVec2(c.x + radius, c.y + radius), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, radius);
        } else {
            dl->AddCircleFilled(c, radius, GuiCards::typeColor(type.id), 20);
            if (!type.name.empty()) {
                const char letter[2] = { static_cast<char>(std::toupper(static_cast<unsigned char>(type.name[0]))), 0 };
                GuiCards::centeredText(dl, c, IM_COL32(255, 255, 255, 255), letter, 0.7f);
            }
        }
        dl->AddCircle(c, radius, IM_COL32(0, 0, 0, 150), 20, 1.5f);
    }
}
