#pragma once
#include <cmath>
#include "imgui.h"
#include "../../../engine/world/entities/pokeball.hpp"
#include "../../../engine/world/style/itemStyle.hpp"
#include "../../../engine/world/style/pokemonStyle.hpp"
#include "../../../models/gameData.hpp"

// Iconos de objetos y pokémon dibujados con ImDrawList. Es el único sitio donde se define su aspecto en la interfaz.
namespace ItemIcon {
    namespace detail {
        inline ImU32 color(const DirectX::XMFLOAT3& tone, float alpha, float shade = 1.0f) {
            const auto channel = [&](float v) { return static_cast<int>((std::min)(v * shade, 1.0f) * 255.0f); };
            return IM_COL32(channel(tone.x), channel(tone.y), channel(tone.z), static_cast<int>(255.0f * alpha));
        }
    }

    // Pokéball: mitad de color, mitad blanca, con banda y botón central.
    inline void ball(ImDrawList* dl, const ImVec2& c, float r, const DirectX::XMFLOAT3& tone, float alpha = 1.0f) {
        const int a = static_cast<int>(255.0f * alpha);
        const ImU32 white = IM_COL32(240, 240, 240, a);
        const ImU32 dark = IM_COL32(20, 20, 25, a);

        dl->AddCircleFilled(c, r, white, 28);
        dl->PathArcTo(c, r, 3.1415927f, 6.2831853f, 20);
        dl->PathFillConvex(detail::color(tone, alpha));
        dl->AddCircle(c, r, dark, 28, 2.0f);
        dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), dark, 2.0f);
        dl->AddCircleFilled(c, r * 0.32f, white, 16);
        dl->AddCircle(c, r * 0.32f, dark, 16, 2.0f);
    }

    // Bloque del color del material (cubo en perspectiva).
    inline void material(ImDrawList* dl, const ImVec2& c, float r, int materialId, float alpha = 1.0f) {
        const DirectX::XMFLOAT3 tone = ItemStyle::materialColor(materialId);
        const float w = r * 0.9f, h = r * 0.52f;
        const ImVec2 top[4] = { { c.x, c.y - r * 0.9f }, { c.x + w, c.y - r * 0.9f + h }, { c.x, c.y - r * 0.9f + h * 2.0f }, { c.x - w, c.y - r * 0.9f + h } };
        const ImVec2 left[4] = { top[3], top[2], { c.x, c.y + r * 0.9f }, { c.x - w, c.y + r * 0.9f - h } };
        const ImVec2 right[4] = { top[2], top[1], { c.x + w, c.y + r * 0.9f - h }, { c.x, c.y + r * 0.9f } };
        dl->AddConvexPolyFilled(top, 4, detail::color(tone, alpha, 1.25f));
        dl->AddConvexPolyFilled(left, 4, detail::color(tone, alpha, 0.85f));
        dl->AddConvexPolyFilled(right, 4, detail::color(tone, alpha, 0.65f));
    }

    // Herramienta: mango y cabeza (hacha o pico).
    inline void tool(ImDrawList* dl, const ImVec2& c, float r, int toolId, float alpha = 1.0f) {
        const ImU32 wood = IM_COL32(140, 92, 46, static_cast<int>(255.0f * alpha));
        const ImU32 metal = IM_COL32(190, 195, 205, static_cast<int>(255.0f * alpha));
        const float t = (std::max)(2.0f, r * 0.14f);
        dl->AddLine(ImVec2(c.x - r * 0.7f, c.y + r * 0.8f), ImVec2(c.x + r * 0.5f, c.y - r * 0.7f), wood, t * 1.4f);
        if (toolId == 3) { // regadera: cuerpo, pico y gotas
            const ImU32 body = IM_COL32(90, 150, 210, static_cast<int>(255.0f * alpha));
            dl->AddRectFilled(ImVec2(c.x - r * 0.7f, c.y - r * 0.2f), ImVec2(c.x + r * 0.3f, c.y + r * 0.7f), body, r * 0.12f);
            dl->AddLine(ImVec2(c.x + r * 0.3f, c.y + r * 0.45f), ImVec2(c.x + r * 0.85f, c.y - r * 0.2f), body, t * 1.6f);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.9f, c.y + r * 0.1f), r * 0.07f, body);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.95f, c.y + r * 0.4f), r * 0.07f, body);
        } else if (toolId == 1) { // hacha
            const ImVec2 head[4] = { { c.x + r * 0.1f, c.y - r * 0.95f }, { c.x + r * 0.85f, c.y - r * 0.55f }, { c.x + r * 0.7f, c.y + r * 0.05f }, { c.x + r * 0.15f, c.y - r * 0.35f } };
            dl->AddConvexPolyFilled(head, 4, metal);
        } else {           // pico
            dl->PathArcTo(ImVec2(c.x + r * 0.5f, c.y - r * 0.2f), r * 0.85f, 3.9f, 5.5f, 12);
            dl->PathStroke(metal, 0, t * 1.8f);
        }
    }

    // Caramelo (sube de nivel) o vitamina (sube EVs).
    inline void training(ImDrawList* dl, const ImVec2& c, float r, TrainingItem::Effect effect, float alpha = 1.0f) {
        const int a = static_cast<int>(255.0f * alpha);
        if (effect == TrainingItem::Effect::LEVEL) {
            const ImU32 wrap = IM_COL32(120, 200, 255, a);
            dl->AddTriangleFilled(ImVec2(c.x - r * 0.55f, c.y), ImVec2(c.x - r * 1.0f, c.y - r * 0.4f), ImVec2(c.x - r * 1.0f, c.y + r * 0.4f), wrap);
            dl->AddTriangleFilled(ImVec2(c.x + r * 0.55f, c.y), ImVec2(c.x + r * 1.0f, c.y - r * 0.4f), ImVec2(c.x + r * 1.0f, c.y + r * 0.4f), wrap);
            dl->AddCircleFilled(c, r * 0.62f, IM_COL32(90, 160, 240, a), 24);
            dl->AddCircle(c, r * 0.62f, IM_COL32(230, 245, 255, a), 24, 2.0f);
            dl->AddLine(ImVec2(c.x - r * 0.3f, c.y - r * 0.15f), ImVec2(c.x + r * 0.3f, c.y - r * 0.15f), IM_COL32(255, 255, 255, a), 2.0f);
        } else {
            dl->AddRectFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.9f), ImVec2(c.x + r * 0.35f, c.y + r * 0.9f), IM_COL32(235, 235, 240, a), r * 0.35f);
            dl->AddRectFilled(ImVec2(c.x - r * 0.35f, c.y), ImVec2(c.x + r * 0.35f, c.y + r * 0.9f), IM_COL32(230, 70, 80, a), r * 0.35f, ImDrawFlags_RoundCornersBottom);
            dl->AddRect(ImVec2(c.x - r * 0.35f, c.y - r * 0.9f), ImVec2(c.x + r * 0.35f, c.y + r * 0.9f), IM_COL32(20, 20, 25, a), r * 0.35f, 0, 2.0f);
        }
    }

    // Pokémon: cubo del color de su especie con morro (igual que en el mundo).
    inline void creature(ImDrawList* dl, const ImVec2& c, float r, int speciesId, float alpha = 1.0f, float darken = 0.0f) {
        const DirectX::XMFLOAT3 tone = PokemonStyle::color(speciesId);
        const float keep = 1.0f - darken; // darken = 1 -> silueta casi negra (pokémon sin ver)
        const DirectX::XMFLOAT3 shaded = { tone.x * keep, tone.y * keep, tone.z * keep };
        const auto fade = [&](int r8, int g8, int b8) { return IM_COL32(static_cast<int>(r8 * keep), static_cast<int>(g8 * keep), static_cast<int>(b8 * keep), static_cast<int>(255.0f * alpha)); };
        dl->AddRectFilled(ImVec2(c.x - r * 0.8f, c.y - r * 0.8f), ImVec2(c.x + r * 0.8f, c.y + r * 0.8f), detail::color(shaded, alpha), r * 0.15f);
        dl->AddRectFilled(ImVec2(c.x + r * 0.35f, c.y - r * 0.1f), ImVec2(c.x + r * 0.95f, c.y + r * 0.35f), fade(255, 230, 40), r * 0.08f);
        dl->AddCircleFilled(ImVec2(c.x - r * 0.2f, c.y - r * 0.25f), r * 0.1f, IM_COL32(20, 20, 25, static_cast<int>(255.0f * alpha)));
    }

    // Icono de cualquier objeto del inventario, según su categoría.
    inline void item(ImDrawList* dl, const ImVec2& c, float r, const Item& item, const GameData& data, float alpha = 1.0f) {
        switch (item.category) {
            case ItemCategory::POKEBALL: if (const PokeballType* type = data.ball(item.refId)) ball(dl, c, r, PokeballStyle::color(type->id), alpha); break;
            case ItemCategory::MATERIAL: material(dl, c, r, item.refId, alpha); break;
            case ItemCategory::TOOL:     tool(dl, c, r, item.refId, alpha); break;
            case ItemCategory::TRAINING: if (const TrainingItem* type = data.training(item.refId)) training(dl, c, r, type->effect, alpha); break;
            default: break;
        }
    }

    // Color de fondo de las casillas de cada categoría de objeto.
    inline ImU32 categoryTone(ItemCategory category) {
        switch (category) {
            case ItemCategory::POKEBALL: return IM_COL32(52, 96, 190, 255);
            case ItemCategory::MATERIAL: return IM_COL32(196, 128, 48, 255);
            case ItemCategory::TOOL:     return IM_COL32(120, 76, 190, 255);
            case ItemCategory::TRAINING: return IM_COL32(40, 150, 120, 255);
            default:                     return IM_COL32(90, 90, 100, 255);
        }
    }

    // Icono representativo de una categoría (pestañas del inventario).
    inline void category(ImDrawList* dl, const ImVec2& c, float r, ItemCategory category) {
        switch (category) {
            case ItemCategory::POKEBALL: ball(dl, c, r, { 0.90f, 0.20f, 0.20f }); break;
            case ItemCategory::MATERIAL: material(dl, c, r, 2); break;
            case ItemCategory::TOOL:     tool(dl, c, r, 1); break;
            case ItemCategory::TRAINING: training(dl, c, r, TrainingItem::Effect::LEVEL); break;
            default: break;
        }
    }
}
