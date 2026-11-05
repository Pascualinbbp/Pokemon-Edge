#pragma once
#include <unordered_map>
#include <DirectXMath.h>

// Colores que viven en la base de datos (columna color de type y material). DatabaseManager los registra al cargar los datos de juego
// y los estilos los consultan por id, sin tener que recibir GameData.
namespace StyleRegistry {
    inline std::unordered_map<int, int>& typeColors()     { static std::unordered_map<int, int> map; return map; }
    inline std::unordered_map<int, int>& materialColors() { static std::unordered_map<int, int> map; return map; }

    inline DirectX::XMFLOAT3 unpack(int rgb) { return { ((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f }; }

    inline DirectX::XMFLOAT3 lookup(const std::unordered_map<int, int>& map, int id, const DirectX::XMFLOAT3& fallback) {
        const auto it = map.find(id);
        return it == map.end() || it->second < 0 ? fallback : unpack(it->second);
    }
}
