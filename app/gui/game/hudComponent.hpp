#pragma once
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "minimapComponent.hpp"
#include "../style/guiPrompts.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../style/icons/teamCircle.hpp"
#include "../../engine/core/gameStatus.hpp"
#include "../../engine/core/inputDevice.hpp"
#include "../../engine/world/rules/captureRules.hpp"
#include "../../engine/world/entities/pokeball.hpp"

namespace HudComponent {
    namespace detail {
        // Texto y color de cada aviso, en el orden de Notice (NONE no se dibuja).
        struct NoticeStyle { const char* text; ImU32 color; };
        inline const NoticeStyle NOTICES[] = {
            { "", 0 },
            { "¡CAPTURADO!",                IM_COL32(80, 220, 110, 255) },
            { "¡SE HA ESCAPADO!",           IM_COL32(235, 70, 60, 255) },
            { "¡LANZAMIENTO CON SUERTE!",   IM_COL32(255, 215, 70, 255) },
            { "¡SUPER SUERTE!",             IM_COL32(150, 235, 255, 255) },
            { "¡SIN UNIDADES!",             IM_COL32(255, 150, 40, 255) },
            { "¡HAS OBTENIDO!",             IM_COL32(255, 215, 70, 255) },
            { "¡SUBES DE NIVEL!",           IM_COL32(120, 220, 255, 255) },
        };

        // Color según la probabilidad de captura: verde muy alta, amarillo buena, naranja dudosa, rojo muy baja.
        inline ImU32 chanceColor(int percent, int alpha) {
            if (percent >= 70) return IM_COL32(80, 220, 110, alpha);
            if (percent >= 45) return IM_COL32(240, 220, 70, alpha);
            if (percent >= 25) return IM_COL32(255, 150, 40, alpha);
            return IM_COL32(235, 70, 60, alpha);
        }

        inline void centered(ImDrawList* dl, ImVec2 center, const char* text, float scale, ImU32 color) {
            ImFont* font = ImGui::GetFont();
            const float size = ImGui::GetFontSize() * scale;
            const ImVec2 extent = font->CalcTextSizeA(size, 1.0e9f, 0.0f, text);
            const ImVec2 pos(center.x - extent.x * 0.5f, center.y);
            dl->AddText(font, size, ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 200), text);
            dl->AddText(font, size, pos, color, text);
        }

        // Punto del mundo en pantalla. false si queda detrás de la cámara.
        inline bool project(const GameStatus& status, const DirectX::XMFLOAT3& world, ImVec2& out) {
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            DirectX::XMFLOAT4 clip;
            DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(DirectX::XMVectorSet(world.x, world.y, world.z, 1.0f), DirectX::XMLoadFloat4x4(&status.viewProj)));
            if (clip.w <= 0.1f) return false;
            out = ImVec2((clip.x / clip.w * 0.5f + 0.5f) * screen.x, (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * screen.y);
            return true;
        }

        // Nombre de cada pokémon visible, proyectado sobre su cabeza.
        inline void drawNameTags(ImDrawList* dl, const GameStatus& status) {
            for (const NameTag& tag : status.nameTags) {
                ImVec2 at;
                if (project(status, tag.position, at)) centered(dl, ImVec2(at.x, at.y - ImGui::GetFontSize()), tag.text.c_str(), 1.0f, tag.shiny ? IM_COL32(255, 220, 90, 255) : IM_COL32(255, 255, 255, 235));
            }
        }

        // Ayuda para interactuar, sobre lo que se interactúa (cofre, recurso, objeto suelto, máquina...), y aviso de lo que falta.
        inline void drawWorldPrompts(ImDrawList* dl, const GameStatus& status, InputDevice device) {
            ImVec2 at;
            if (status.interactVerb && status.interactTarget && project(status, status.interactPos, at)) {
                char text[96];
                std::snprintf(text, sizeof(text), "%s %s", status.interactVerb, status.interactTarget->c_str());
                constexpr float ICON = 24.0f; // ancho del icono de la tecla o botón
                const float width = ICON + 8.0f + ImGui::CalcTextSize(text).x;
                GuiPrompts::draw(dl, ImVec2(at.x - width * 0.5f, at.y - ICON * 0.5f), GuiPrompts::Action::INTERACT, text, device);
            }
            if (status.missingSkill && project(status, status.missingPos, at)) {
                char text[112];
                std::snprintf(text, sizeof(text), "Necesitas %s de nivel %d (herramienta o pokémon)", status.missingSkill->c_str(), status.missingLevel);
                centered(dl, at, text, 1.0f, IM_COL32(255, 150, 40, 255));
            }
        }

        // Aviso de autoguardado (esquina inferior derecha): panel translúcido con el contorno de una pokéball
        // que gira y el texto "Guardando...". alpha (0..1) controla el fundido de entrada y salida.
        inline void drawSaving(ImDrawList* dl, float alpha) {
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            const auto a = [alpha](int value) { return static_cast<int>(value * alpha); };

            constexpr float RADIUS = 15.0f;
            constexpr float BUTTON = 5.5f;
            constexpr float MARGIN = 24.0f;
            constexpr float PAD_X = 16.0f;
            constexpr float PAD_Y = 11.0f;
            constexpr float GAP = 12.0f;
            constexpr float TEXT_SCALE = 1.1f;

            ImFont* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize() * TEXT_SCALE;
            const char* text = "Guardando...";
            const ImVec2 textSize = font->CalcTextSizeA(fontSize, 1.0e9f, 0.0f, text);

            const float panelW = PAD_X * 2.0f + RADIUS * 2.0f + GAP + textSize.x;
            const float panelH = PAD_Y * 2.0f + RADIUS * 2.0f;
            const ImVec2 p1(screen.x - MARGIN, screen.y - MARGIN);
            const ImVec2 p0(p1.x - panelW, p1.y - panelH);

            dl->AddRectFilled(p0, p1, IM_COL32(12, 12, 18, a(190)), 12.0f);
            dl->AddRect(p0, p1, IM_COL32(255, 255, 255, a(70)), 12.0f, 0, 1.5f);

            const ImVec2 c(p0.x + PAD_X + RADIUS, p0.y + panelH * 0.5f);
            const ImU32 color = IM_COL32(245, 245, 245, a(255));
            const ImU32 shadow = IM_COL32(0, 0, 0, a(160));

            const ImVec2 textPos(c.x + RADIUS + GAP, c.y - textSize.y * 0.5f);
            dl->AddText(font, fontSize, ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), shadow, text);
            dl->AddText(font, fontSize, textPos, color, text);

            // Solo bordes: aro exterior, ecuador giratorio con hueco para el botón central, y el aro del botón.
            const float angle = static_cast<float>(ImGui::GetTime()) * 5.0f;
            const ImVec2 d(std::cos(angle), std::sin(angle));
            dl->AddCircle(c, RADIUS, shadow, 32, 4.5f);
            dl->AddCircle(c, RADIUS, color, 32, 2.5f);
            dl->AddLine(ImVec2(c.x - d.x * RADIUS, c.y - d.y * RADIUS), ImVec2(c.x - d.x * BUTTON, c.y - d.y * BUTTON), color, 2.5f);
            dl->AddLine(ImVec2(c.x + d.x * BUTTON, c.y + d.y * BUTTON), ImVec2(c.x + d.x * RADIUS, c.y + d.y * RADIUS), color, 2.5f);
            dl->AddCircle(c, BUTTON, color, 16, 2.5f);
        }

        // Selector de pokéball (solo al apuntar): todas las del inventario, la equipada resaltada, con sus unidades
        // y el multiplicador de captura. Q / E o la rueda en teclado; L1 / R1 en mando.
        inline void drawBallSelector(ImDrawList* dl, const GameStatus& status, InputDevice device) {
            const Inventory& inventory = *status.inventory;
            const int count = inventory.ballCount();
            const float alpha = status.aimBlend;
            const int a = static_cast<int>(255.0f * alpha);
            const ImVec2 screen = ImGui::GetIO().DisplaySize;

            constexpr float SPACING = 66.0f, SELECTED_RADIUS = 22.0f, OTHER_RADIUS = 15.0f;
            const float centerY = screen.y - 110.0f;
            const float startX = screen.x * 0.5f - SPACING * static_cast<float>(count - 1) * 0.5f;
            char buffer[96];

            for (int i = 0; i < count; ++i) {
                const bool selected = i == inventory.selectedIndex();
                const ImVec2 c(startX + SPACING * static_cast<float>(i), centerY);
                if (selected) dl->AddCircle(c, SELECTED_RADIUS + 6.0f, IM_COL32(255, 255, 255, a), 32, 2.5f);
                ItemIcon::ball(dl, c, selected ? SELECTED_RADIUS : OTHER_RADIUS, PokeballStyle::color(inventory.ball(i).type.id), selected ? alpha : alpha * 0.55f);
                if (!selected) {
                    std::snprintf(buffer, sizeof(buffer), "x%d", inventory.ball(i).count);
                    centered(dl, ImVec2(c.x, c.y + OTHER_RADIUS + 6.0f), buffer, 0.8f, IM_COL32(220, 220, 220, static_cast<int>(a * 0.7f)));
                }
            }

            const Inventory::BallSlot slot = inventory.selectedBall();
            std::snprintf(buffer, sizeof(buffer), "%s   x%d   Captura x%.1f", slot.type.name.c_str(), slot.count, slot.type.captureMultiplier);
            const ImU32 textColor = slot.count > 0 ? IM_COL32(255, 255, 255, a) : IM_COL32(235, 70, 60, a);
            const float nameY = centerY + SELECTED_RADIUS + 26.0f;
            centered(dl, ImVec2(screen.x * 0.5f, nameY), buffer, 1.0f, textColor);

            if (count > 1) {
                const bool pad = isGamepad(device);
                const float keyY = centerY - 14.0f;
                GuiDraw::keycap(dl, ImVec2(startX - SELECTED_RADIUS - 52.0f, keyY), pad ? "L1" : "Q", 28.0f, 28.0f);
                GuiDraw::keycap(dl, ImVec2(startX + SPACING * static_cast<float>(count - 1) + SELECTED_RADIUS + 24.0f, keyY), pad ? "R1" : "E", 28.0f, 28.0f);
            }
        }

        // Ayudas de controles contextuales (a la izquierda): las básicas siempre y, según lo que se haga, las del modo
        // captura o la de interactuar con lo que hay cerca. El icono cambia solo según el dispositivo en uso.
        inline void drawHints(ImDrawList* dl, const GameStatus& status, InputDevice device) {
            using GuiPrompts::Action;
            struct Hint {
                Action action;
                char text[64];
            };
            Hint hints[9];
            int count = 0;
            const auto add = [&](Action action, const char* text) {
                hints[count].action = action;
                std::snprintf(hints[count].text, sizeof(hints[count].text), "%s", text);
                ++count;
            };

            if (status.aiming) {
                add(Action::THROW, "Lanzar Pokéball");
                if (status.inventory && status.inventory->ballCount() > 1) add(Action::BALL_SWITCH, "Cambiar de Pokéball");
                if (status.canLock) add(Action::LOCK, status.locked ? "Cambiar objetivo" : "Fijar objetivo");
                add(Action::AIM, "Salir del modo captura");
            } else {
                add(Action::JUMP, "Saltar");
                add(Action::CROUCH, "Agacharse");
                add(Action::SPRINT, "Correr");
                add(Action::AIM, "Modo captura");
                if (status.team.size() > 1) add(Action::BALL_SWITCH, "Cambiar de pokémon");
                if (!status.team.empty()) add(Action::MODE, "Modo del pokémon");
                add(Action::INVENTORY, "Mochila");
                add(Action::PAUSE, "Pausa");
            }

            constexpr float ROW = 32.0f;
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            ImVec2 pos(24.0f, screen.y * 0.5f - ROW * static_cast<float>(count) * 0.5f);
            for (int i = 0; i < count; ++i) {
                GuiPrompts::draw(dl, pos, hints[i].action, hints[i].text, device);
                pos.y += ROW;
            }
        }

        // Hábitat y clima de donde está el jugador y modo del pokémon que lo acompaña (arriba a la izquierda).
        inline void drawLocation(ImDrawList* dl, const GameStatus& status) {
            const ImVec2 at(MinimapComponent::center().x - 84.0f, MinimapComponent::bottom() + 4.0f);
            if (!status.locationText.empty()) dl->AddText(at, IM_COL32(255, 255, 255, 235), status.locationText.c_str());
            if (status.companionMode) {
                const float y = at.y + (status.locationText.empty() ? 0.0f : ImGui::GetFontSize() + 4.0f);
                char text[48];
                std::snprintf(text, sizeof(text), "Modo: %s", status.companionMode);
                dl->AddText(ImVec2(at.x, y), IM_COL32(255, 215, 90, 235), text);
            }
        }

        // Nivel del jugador y su experiencia (arriba a la derecha).
        inline void drawLevel(ImDrawList* dl, const GameStatus& status) {
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            constexpr float WIDTH = 150.0f;
            char text[48];
            std::snprintf(text, sizeof(text), "Nv. %d / %d", status.playerLevel, status.levelCap);
            const ImVec2 at(screen.x - WIDTH - 24.0f, 14.0f);
            dl->AddText(at, IM_COL32(255, 255, 255, 235), text);
            const ImVec2 bar(at.x, at.y + ImGui::GetFontSize() + 4.0f);
            dl->AddRectFilled(bar, ImVec2(bar.x + WIDTH, bar.y + 6.0f), IM_COL32(255, 255, 255, 50), 3.0f);
            dl->AddRectFilled(bar, ImVec2(bar.x + WIDTH * status.playerXp, bar.y + 6.0f), IM_COL32(120, 220, 255, 255), 3.0f);
        }

        // Equipo en círculos verticales a la derecha, siempre en el mismo orden (el del menú de pokémon). El que acompaña
        // al jugador va resaltado. En cada círculo: el pokémon (más adelante su sprite), el nivel abajo, sus tipos
        // abajo a la izquierda, la pokéball con la que se capturó abajo a la derecha y la tecla para sacarlo.
        inline void drawTeam(ImDrawList* dl, const GameStatus& status) {
            if (status.team.empty() || !status.data) return;
            constexpr float RADIUS = 30.0f, GAP = 20.0f;
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            const float total = static_cast<float>(status.team.size()) * (RADIUS * 2.0f + GAP) - GAP;
            const float x = screen.x - RADIUS - 28.0f;
            float y = screen.y * 0.5f - total * 0.5f + RADIUS;
            int number = 1;
            for (const TeamEntry& entry : status.team) {
                TeamCircle::draw(dl, ImVec2(x, y), RADIUS, *status.data, *entry.species, entry.level, entry.shiny, entry.ballId, entry.active, number++);
                y += RADIUS * 2.0f + GAP;
            }
        }

        // Mira redonda (anillo con punto central); cambia de color al apuntar a un pokémon en rango.
        inline void drawCrosshair(ImDrawList* dl, const GameStatus& status) {
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const ImVec2 c(size.x * 0.5f, size.y * 0.5f);
            const int a = static_cast<int>(status.aimBlend * 255.0f);
            const ImU32 color = status.hasAimTarget ? chanceColor(status.chancePercent, a) : IM_COL32(255, 255, 255, a);
            const ImU32 shadow = IM_COL32(0, 0, 0, static_cast<int>(a * 0.6f));
            constexpr float RADIUS = 14.0f;

            dl->AddCircle(c, RADIUS, shadow, 0, 4.0f);
            dl->AddCircle(c, RADIUS, color, 0, 2.0f);
            dl->AddCircleFilled(c, 3.0f, shadow);
            dl->AddCircleFilled(c, 2.0f, color);

            if (status.hasAimTarget) {
                char buffer[32];
                std::snprintf(buffer, sizeof(buffer), "%d%%", status.chancePercent);
                centered(dl, ImVec2(c.x, c.y + RADIUS + 8.0f), buffer, 1.6f, color);

                float y = c.y + RADIUS + 8.0f + ImGui::GetFontSize() * 1.6f + 4.0f;
                if (status.behind) {
                    std::snprintf(buffer, sizeof(buffer), "ESPALDA x%.2f", CaptureRules::BACK_MULTIPLIER);
                    centered(dl, ImVec2(c.x, y), buffer, 0.9f, IM_COL32(120, 200, 255, a));
                    y += ImGui::GetFontSize() * 0.9f + 2.0f;
                }
                if (status.hidden) {
                    std::snprintf(buffer, sizeof(buffer), "SIGILO x%.2f", CaptureRules::STEALTH_MULTIPLIER);
                    centered(dl, ImVec2(c.x, y), buffer, 0.9f, IM_COL32(120, 200, 255, a));
                }
            }

            if (status.locked) centered(dl, ImVec2(c.x, c.y - RADIUS - 8.0f - ImGui::GetFontSize() * 0.9f), "FIJADO", 0.9f, IM_COL32(255, 255, 255, a));
        }
    }

    // La pausa con ESC la gestiona la escena (ESC sale primero del modo lanzamiento).
    // saving: opacidad del aviso de autoguardado (0 = oculto, 1 = totalmente visible).
    inline void render(InputDevice device, const GameStatus& status, float saving) {
        ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
        ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (saving > 0.0f) detail::drawSaving(dl, saving);

        if (status.aimBlend > 0.05f) {
            detail::drawCrosshair(dl, status);
            if (status.inventory && status.inventory->hasBalls()) detail::drawBallSelector(dl, status, device);
        }

        detail::drawNameTags(dl, status);
        detail::drawLevel(dl, status);
        MinimapComponent::draw(dl, status);
        detail::drawLocation(dl, status);
        detail::drawTeam(dl, status);
        detail::drawHints(dl, status, device);

        detail::drawWorldPrompts(dl, status, device);

        if (status.notice != Notice::NONE) {
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            const detail::NoticeStyle& style = detail::NOTICES[static_cast<int>(status.notice)];
            detail::centered(dl, ImVec2(size.x * 0.5f, size.y * 0.2f), style.text, 1.8f, style.color);
            if (status.noticeText) {
                detail::centered(dl, ImVec2(size.x * 0.5f, size.y * 0.2f + ImGui::GetFontSize() * 1.8f + 8.0f),
                    status.noticeText->c_str(), 1.5f, IM_COL32(255, 255, 255, 255));
            }
        }
    }
}
