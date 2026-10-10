#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

// Aspecto de cada pokémon y tipo (por id de la base de datos). Es el único sitio donde se definen sus colores.
namespace PokemonStyle {
    // Color propio de cada especie sin color a mano: el tono recorre el círculo cromático con el id (ángulo áureo), así que dos
    // especies seguidas nunca se parecen.
    inline DirectX::XMFLOAT3 hue(int speciesId) {
        const float h = std::fmod(static_cast<float>(speciesId) * 0.61803399f, 1.0f) * 6.0f;
        const float s = 0.55f, v = 0.88f;
        const float f = h - std::floor(h);
        const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
        switch (static_cast<int>(h)) {
            case 0:  return { v, t, p };
            case 1:  return { q, v, p };
            case 2:  return { p, v, t };
            case 3:  return { p, q, v };
            case 4:  return { t, p, v };
            default: return { v, p, q };
        }
    }

    // Color del cuerpo del pokémon en el mundo (el orden de pokemon.sql fija los id).
    inline DirectX::XMFLOAT3 color(int speciesId) {
        switch (speciesId) {
            case 1:  return { 0.30f, 0.68f, 0.58f }; // Bulbasaur
            case 2:  return { 0.27f, 0.60f, 0.50f }; // Ivysaur
            case 3:  return { 0.22f, 0.52f, 0.46f }; // Venusaur
            case 4:  return { 0.95f, 0.48f, 0.20f }; // Charmander
            case 5:  return { 0.82f, 0.28f, 0.20f }; // Charmeleon
            case 6:  return { 0.95f, 0.55f, 0.15f }; // Charizard
            case 7:  return { 0.35f, 0.65f, 0.90f }; // Squirtle
            case 8:  return { 0.28f, 0.50f, 0.85f }; // Wartortle
            case 9:  return { 0.22f, 0.40f, 0.75f }; // Blastoise
            default: return hue(speciesId);
        }
    }

    // Color del variocolor: los canales rotados y algo más claros.
    inline DirectX::XMFLOAT3 shiny(const DirectX::XMFLOAT3& c) {
        return { (std::min)(1.0f, c.z * 1.3f + 0.15f), (std::min)(1.0f, c.x * 1.1f + 0.1f), (std::min)(1.0f, c.y * 1.3f + 0.15f) };
    }

    // Color de la etiqueta de cada tipo (RGB 0..255); el orden de la tabla type fija los id.
    inline void typeRgb(int typeId, int& r, int& g, int& b) {
        static constexpr int RGB[][3] = {
            { 168, 168, 120 }, // 1 normal
            { 240, 128, 48 },  // 2 fuego
            { 104, 144, 240 }, // 3 agua
            { 120, 200, 80 },  // 4 planta
            { 248, 208, 48 },  // 5 eléctrico
            { 152, 216, 216 }, // 6 hielo
            { 192, 48, 40 },   // 7 lucha
            { 160, 90, 200 },  // 8 veneno
            { 224, 192, 104 }, // 9 tierra
            { 168, 144, 240 }, // 10 volador
            { 248, 88, 136 },  // 11 psíquico
            { 168, 184, 32 },  // 12 bicho
            { 184, 160, 56 },  // 13 roca
            { 112, 88, 152 },  // 14 fantasma
            { 112, 56, 248 },  // 15 dragón
            { 112, 88, 72 },   // 16 siniestro
            { 184, 184, 208 }, // 17 acero
            { 238, 153, 172 }, // 18 hada
            { 80, 190, 170 },  // 19 sonido
        };
        constexpr int COUNT = static_cast<int>(sizeof(RGB) / sizeof(RGB[0]));
        const int i = (typeId >= 1 && typeId <= COUNT) ? typeId - 1 : 0;
        r = RGB[i][0]; g = RGB[i][1]; b = RGB[i][2];
    }
}
