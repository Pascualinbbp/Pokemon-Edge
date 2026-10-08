#pragma once
#include <cstdint>
#include <string>
#include <DirectXMath.h>

// Aspecto de cada hábitat (único sitio donde se define): color del suelo. Un hábitat sin entrada recibe un color derivado
// de su nombre, así que añadirlo a la base de datos basta para verlo.
namespace HabitatStyle {
    inline DirectX::XMFLOAT3 color(const std::string& name) {
        if (name == "Pradera") return { 0.30f, 0.72f, 0.26f };
        if (name == "Bosque") return { 0.12f, 0.45f, 0.20f };
        if (name == "Montaña") return { 0.48f, 0.46f, 0.44f };
        if (name == "Costa") return { 0.86f, 0.80f, 0.55f };
        if (name == "Desierto") return { 0.82f, 0.66f, 0.36f };
        if (name == "Tundra") return { 0.86f, 0.92f, 0.96f };
        uint32_t hash = 2166136261u;
        for (const char c : name) hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
        return { 0.3f + 0.5f * static_cast<float>(hash & 0xFF) / 255.0f, 0.3f + 0.5f * static_cast<float>((hash >> 8) & 0xFF) / 255.0f,
                 0.3f + 0.5f * static_cast<float>((hash >> 16) & 0xFF) / 255.0f };
    }
}
