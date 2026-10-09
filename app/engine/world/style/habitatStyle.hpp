#pragma once
#include <cstdint>
#include <string>
#include <DirectXMath.h>

// Aspecto de cada hábitat (único sitio donde se define): color del suelo. Un hábitat sin entrada recibe un color derivado
// de su nombre, así que añadirlo a la base de datos basta para verlo.
namespace HabitatStyle {
    inline DirectX::XMFLOAT3 color(const std::string& name) {
        if (name == "Pradera") return { 0.30f, 0.72f, 0.26f };
        if (name == "Bosque de robles") return { 0.16f, 0.52f, 0.18f };
        if (name == "Bosque de pinos") return { 0.08f, 0.36f, 0.24f };
        if (name == "Bosque mixto") return { 0.14f, 0.45f, 0.20f };
        if (name == "Montaña") return { 0.48f, 0.46f, 0.44f };
        if (name == "Playa") return { 0.90f, 0.84f, 0.58f };
        if (name == "Río") return { 0.25f, 0.55f, 0.85f };
        if (name == "Lago") return { 0.15f, 0.40f, 0.72f };
        if (name == "Pantano") return { 0.30f, 0.38f, 0.22f };
        if (name == "Desierto") return { 0.82f, 0.66f, 0.36f };
        if (name == "Tundra") return { 0.86f, 0.92f, 0.96f };
        uint32_t hash = 2166136261u;
        for (const char c : name) hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
        return { 0.3f + 0.5f * static_cast<float>(hash & 0xFF) / 255.0f, 0.3f + 0.5f * static_cast<float>((hash >> 8) & 0xFF) / 255.0f,
                 0.3f + 0.5f * static_cast<float>((hash >> 16) & 0xFF) / 255.0f };
    }
}
