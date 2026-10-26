#pragma once
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "minimapComponent.hpp"
#include "../style/guiPopup.hpp"
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
            { "¡TE HAS DESMAYADO!",         IM_COL32(235, 70, 60, 255) },
        };

        // Color según la probabilidad de captura: verde muy alta, amarillo buena, naranja dudosa, rojo muy baja.
        inline ImU32 chanceColor(int percent, int alpha) {
            if (percent >= 70) return IM_COL32(80, 220, 110, alpha);
            if (percent >= 45) return IM_COL32(240, 220, 70, alpha);
            if (percent >= 25) return IM_COL32(255, 150, 40, alpha);
            return IM_COL32(235, 70, 60, alpha);
        }

        inline void centered(ImDrawList* dl, ImVec2 center, const char* text, float scale, ImU32 color) { GuiPopup::text(dl, center, text, scale, color); }

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

        // Ayuda para interactuar, sobre lo que se interactúa (cofre, recurso...), y aviso de lo que falta.
        inline void drawWorldPrompts(ImDrawList* dl, const GameStatus& status, InputDevice device) {
            ImVec2 at;
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            const bool placed = status.interactOnScreen ? (at = ImVec2(screen.x * 0.5f, screen.y * 0.62f), true) : project(status, status.interactPos, at);
            if (status.interactVerb && status.interactTarget && placed) {
                char text[96];
                std::snprintf(text, sizeof(text), status.interactTarget->empty() ? "%s" : "%s %s", status.interactVerb, status.interactTarget->c_str());
                GuiPopup::at(dl, at, GuiPrompts::Action::INTERACT, text, device, true, status.interactTime);
            }
            if (status.missingSkill && project(status, status.missingPos, at)) {
                char text[112];
                std::snprintf(text, sizeof(text), "Necesitas %s de nivel %d (herramienta o pokémon)", status.missingSkill->c_str(), status.missingLevel);
                GuiPopup::text(dl, at, text, 1.0f, IM_COL32(255, 150, 40, 255));
            }
        }

        // Ayudas inferiores que pide la escena (pesca: Capturar | Liberar; buceo...), centradas sobre la barra de pokéballs.
        inline void drawPrompts(ImDrawList* dl, const GameStatus& status, InputDevice device) {
            if (status.prompts.empty()) return;
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            GuiPopup::row(dl, screen.x * 0.5f, screen.y - 190.0f, status.prompts, device);
        }

        // Medidor de aire (solo bajo el agua o recuperándolo) y temperatura / altura / región, bajo el nivel del jugador.
        // Arco de un anillo desde arriba en el sentido del reloj ('fraction' 0..1 de la vuelta).
        inline void ringArc(ImDrawList* dl, const ImVec2& c, float radius, float from, float to, ImU32 color, float width) {
            if (to <= from) return;
            constexpr float START = -1.5707963f, TURN = 6.2831853f;
            dl->PathArcTo(c, radius, START + TURN * from, START + TURN * to, 48);
            dl->PathStroke(color, 0, width);
        }

        // Círculo del jugador (arriba a la derecha, se abre con la tecla del inventario): alrededor, anillos de vida, experiencia y aire.
        // Sin números: la vida pierde un tramo blanco que se va apagando al recibir daño y brilla en verde, con cruces que suben, al curarse.
        inline void drawVitals(ImDrawList* dl, const GameStatus& status, InputDevice device) {
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            const float dt = ImGui::GetIO().DeltaTime, time = static_cast<float>(ImGui::GetTime());
            constexpr float CORE = 28.0f, HEALTH_R = 36.0f, XP_R = 45.0f, AIR_R = 53.0f;
            ImVec2 c(screen.x - 74.0f, 74.0f);
            const float health = (std::clamp)(status.health, 0.0f, 1.0f);
            static float ghost = 1.0f; // la vida que se acaba de perder, que se apaga despacio
            ghost = (std::max)(health, ghost - dt * 0.35f);
            if (status.hurt > 0.0f) { c.x += std::sin(time * 70.0f) * 2.5f * status.hurt; c.y += std::cos(time * 63.0f) * 2.0f * status.hurt; }

            const ImU32 track = IM_COL32(255, 255, 255, 38);
            dl->AddCircleFilled(c, CORE + 3.0f, IM_COL32(12, 16, 24, 190), 40);
            dl->AddCircle(c, HEALTH_R, track, 48, 7.0f);
            const ImU32 healthColor = health < 0.25f ? IM_COL32(235, 70, 60, 255) : health < 0.5f ? IM_COL32(255, 170, 50, 255) : IM_COL32(90, 220, 120, 255);
            ringArc(dl, c, HEALTH_R, health, ghost, IM_COL32(255, 255, 255, 200), 7.0f);
            ringArc(dl, c, HEALTH_R, 0.0f, health, healthColor, 7.0f);
            if (status.heal > 0.0f) {
                ringArc(dl, c, HEALTH_R, 0.0f, health, IM_COL32(190, 255, 220, static_cast<int>(200.0f * status.heal)), 10.0f * status.heal + 7.0f);
                dl->AddCircle(c, HEALTH_R + 6.0f * (1.0f - status.heal), IM_COL32(120, 255, 190, static_cast<int>(120.0f * status.heal)), 48, 2.0f);
                for (int i = 0; i < 4; ++i) { // cruces que suben
                    const float phase = std::fmod(time * 0.9f + i * 0.25f, 1.0f), x = c.x + (i - 1.5f) * 14.0f, y = c.y + 8.0f - phase * 46.0f, r = 4.0f;
                    const ImU32 plus = IM_COL32(150, 255, 200, static_cast<int>(230.0f * (1.0f - phase) * status.heal));
                    dl->AddLine(ImVec2(x - r, y), ImVec2(x + r, y), plus, 2.0f);
                    dl->AddLine(ImVec2(x, y - r), ImVec2(x, y + r), plus, 2.0f);
                }
            }
            if (status.hurt > 0.0f) dl->AddCircle(c, HEALTH_R, IM_COL32(255, 60, 40, static_cast<int>(160.0f * status.hurt)), 48, 11.0f);
            if (status.burning) dl->AddCircle(c, HEALTH_R + 2.0f, IM_COL32(255, static_cast<int>(120 + 60 * std::sin(time * 14.0f)), 30, 170), 48, 4.0f);

            // Experiencia: anillo propio, igual que el de la vida (con el tramo ya ganado en dorado).
            dl->AddCircle(c, XP_R, track, 48, 5.0f);
            ringArc(dl, c, XP_R, 0.0f, (std::clamp)(status.playerXp, 0.0f, 1.0f), IM_COL32(250, 205, 70, 255), 5.0f);
            if (status.showOxygen) {
                const float air = (std::clamp)(status.oxygen, 0.0f, 1.0f);
                const bool low = status.exhausted || air < 0.25f;
                dl->AddCircle(c, AIR_R, track, 48, 3.0f);
                ringArc(dl, c, AIR_R, 0.0f, air, low ? IM_COL32(235, 70, 60, static_cast<int>(150 + 105 * std::sin(time * 12.0f))) : IM_COL32(90, 190, 255, 255), 3.0f);
            }

            // El jugador: cabeza y hombros; se tiñe de rojo con el daño y de verde al curarse.
            const ImU32 body = status.hurt > 0.0f ? IM_COL32(255, 120, 110, 255) : status.heal > 0.0f ? IM_COL32(170, 255, 205, 255) : IM_COL32(235, 238, 245, 255);
            dl->AddCircleFilled(ImVec2(c.x, c.y - 7.0f), 8.5f, body, 24);
            dl->PathArcTo(ImVec2(c.x, c.y + 20.0f), 16.0f, 3.1415927f * 1.08f, 3.1415927f * 1.92f, 20);
            dl->PathFillConvex(body);
            const bool pad = isGamepad(device);
            GuiDraw::keycap(dl, ImVec2(c.x - (pad ? 18.0f : 9.0f), c.y + AIR_R + 7.0f), pad ? "View" : "I", pad ? 36.0f : 18.0f, 20.0f);

            // Temperatura y altura debajo; el aviso de región y los estados a su lado.
            char text[64];
            std::snprintf(text, sizeof(text), "%.0f °C   %.0f m", status.temperature, status.altitude);
            const ImU32 tone = status.temperature >= 45.0f ? IM_COL32(255, 120, 60, 255) : status.temperature >= 32.0f ? IM_COL32(255, 190, 90, 255) : status.temperature <= 0.0f ? IM_COL32(170, 220, 255, 255) : status.temperature < 10.0f ? IM_COL32(200, 230, 255, 235) : IM_COL32(255, 255, 255, 220);
            const float width = ImGui::CalcTextSize(text).x;
            float y = c.y + AIR_R + 34.0f;
            dl->AddText(ImVec2(c.x + HEALTH_R - width, y), tone, text);
            y += ImGui::GetFontSize() + 4.0f;
            const auto line = [&](const char* label, ImU32 color) {
                dl->AddText(ImVec2(c.x + HEALTH_R - ImGui::CalcTextSize(label).x, y), color, label);
                y += ImGui::GetFontSize() + 4.0f;
            };
            if (status.burning) line("¡Quemado!", IM_COL32(255, 120, 40, 255));
            if (status.spring == 1) line("Aguas termales", IM_COL32(120, 235, 215, 255));
            else if (status.spring == 2) line("¡Demasiado tiempo en el agua!", IM_COL32(255, 110, 90, 255));
            if (status.exhausted) line("¡Sin aire!", IM_COL32(255, 90, 80, 255));
            if (status.regionName) line(status.regionName->c_str(), IM_COL32(255, 215, 90, 235));
        }

        // Tinte de pantalla: azul bajo el agua, oscuridad en cuevas y minas.
        // Viñeta de pantalla: un borde que se desvanece hacia el centro (daño, curación, quemadura).
        inline void vignette(ImDrawList* dl, const ImVec2& screen, int r, int g, int b, float strength) {
            if (strength <= 0.0f) return;
            const int a = static_cast<int>((std::clamp)(strength, 0.0f, 1.0f) * 150.0f);
            const ImU32 edge = IM_COL32(r, g, b, a), none = IM_COL32(r, g, b, 0);
            const float w = screen.x * 0.18f, h = screen.y * 0.22f;
            dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(screen.x, h), edge, edge, none, none);
            dl->AddRectFilledMultiColor(ImVec2(0, screen.y - h), ImVec2(screen.x, screen.y), none, none, edge, edge);
            dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, screen.y), edge, none, none, edge);
            dl->AddRectFilledMultiColor(ImVec2(screen.x - w, 0), ImVec2(screen.x, screen.y), none, edge, edge, none);
        }

        // Tinte de pantalla: azul bajo el agua, oscuridad en cuevas y minas, y los efectos de daño (rojo), quemadura (naranja), falta de aire y curación (verde).
        inline void drawTint(ImDrawList* dl, const GameStatus& status) {
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            const float time = static_cast<float>(ImGui::GetTime());
            if (status.underwater) dl->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(20, 70, 120, 110));
            if (status.darkness > 0.0f) dl->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(0, 0, 0, static_cast<int>(120.0f * status.darkness))); // cueva y mina: oscuras
            if (status.burning) vignette(dl, screen, 255, 110, 20, 0.55f + 0.25f * std::sin(time * 13.0f));
            if (status.exhausted && status.underwater) vignette(dl, screen, 20, 40, 120, 0.7f + 0.2f * std::sin(time * 9.0f));
            if (status.hurt > 0.0f) {
                dl->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(220, 20, 20, static_cast<int>(60.0f * status.hurt)));
                vignette(dl, screen, 230, 20, 20, status.hurt * 1.2f);
            }
            if (status.heal > 0.0f) vignette(dl, screen, 70, 235, 160, status.heal * 0.8f);
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
            std::vector<int> shown; // solo las que se tienen (si no hay ninguna, no se muestra nada)
            for (int i = 0; i < inventory.ballCount(); ++i) if (inventory.owned(i)) shown.push_back(i);
            if (shown.empty()) return;
            const int count = static_cast<int>(shown.size());
            const float alpha = status.aimBlend;
            const int a = static_cast<int>(255.0f * alpha);
            const ImVec2 screen = ImGui::GetIO().DisplaySize;

            constexpr float SPACING = 66.0f, SELECTED_RADIUS = 22.0f, OTHER_RADIUS = 15.0f;
            const float centerY = screen.y - 110.0f;
            const float startX = screen.x * 0.5f - SPACING * static_cast<float>(count - 1) * 0.5f;
            char buffer[96];

            for (int k = 0; k < count; ++k) {
                const int i = shown[k];
                const bool selected = i == inventory.selectedIndex();
                const ImVec2 c(startX + SPACING * static_cast<float>(k), centerY);
                if (selected) dl->AddCircle(c, SELECTED_RADIUS + 6.0f, IM_COL32(255, 255, 255, a), 32, 2.5f);
                ItemIcon::ball(dl, c, selected ? SELECTED_RADIUS : OTHER_RADIUS, PokeballStyle::look(inventory.ball(i).type.id), selected ? alpha : alpha * 0.55f);
                if (!selected) {
                    std::snprintf(buffer, sizeof(buffer), "x%d", inventory.ball(i).count);
                    centered(dl, ImVec2(c.x, c.y + OTHER_RADIUS + 6.0f), buffer, 0.8f, IM_COL32(220, 220, 220, static_cast<int>(a * 0.7f)));
                }
            }

            const Inventory::BallSlot slot = inventory.selectedBall();
            std::snprintf(buffer, sizeof(buffer), "%s   x%d   Captura %s", slot.type.name.c_str(), slot.count, slot.type.ratio().c_str());
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
                if (status.inventory && status.inventory->ownedCount() > 1) add(Action::BALL_SWITCH, "Cambiar de Pokéball");
                if (status.canLock) add(Action::LOCK, status.locked ? "Cambiar objetivo" : "Fijar objetivo");
                add(Action::AIM, "Salir del modo captura");
            } else {
                add(Action::JUMP, "Saltar");
                add(Action::CROUCH, "Agacharse");
                add(Action::SPRINT, "Correr");
                add(Action::AIM, "Modo captura");
                if (status.team.size() > 1) add(Action::BALL_SWITCH, "Cambiar de pokémon");
                if (!status.team.empty()) add(Action::MODE, "Modo del pokémon");
                add(Action::INVENTORY, "Jugador");
                add(Action::PAUSE, "Pausa");
            }

            constexpr float ROW = 32.0f;
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            ImVec2 pos(24.0f, screen.y - 20.0f - ROW * static_cast<float>(count)); // abajo a la izquierda: fuera del centro de la vista
            for (int i = 0; i < count; ++i) {
                GuiPrompts::draw(dl, pos, hints[i].action, hints[i].text, device);
                pos.y += ROW;
            }
        }

        // "clima - hábitat" y modo del pokémon que acompaña, centrados bajo el minimapa.
        inline void drawLocation(ImDrawList* dl, const GameStatus& status) {
            ImVec2 at(MinimapComponent::center().x, MinimapComponent::bottom() + 2.0f);
            if (status.habitatName) {
                char text[96];
                std::snprintf(text, sizeof(text), "%s - %s", status.weatherName ? status.weatherName->c_str() : "Despejado", status.habitatName->c_str());
                centered(dl, at, text, 1.1f, IM_COL32(255, 255, 255, 240));
                at.y += ImGui::GetFontSize() * 1.1f + 3.0f;
            }
            if (status.companionMode) {
                char text[48];
                std::snprintf(text, sizeof(text), "Modo: %s", status.companionMode);
                centered(dl, at, text, 1.0f, IM_COL32(255, 215, 90, 235));
            }
        }

        // Nivel del jugador, justo debajo del círculo.
        inline void drawLevel(ImDrawList* dl, const GameStatus& status) {
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            char text[32];
            std::snprintf(text, sizeof(text), "Nv. %d", status.playerLevel);
            const ImVec2 size = ImGui::CalcTextSize(text);
            dl->AddText(ImVec2(screen.x - 74.0f - size.x * 0.5f, 74.0f + 53.0f + 14.0f), IM_COL32(255, 255, 255, 235), text);
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
        ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));
        ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (saving > 0.0f) detail::drawSaving(dl, saving);

        if (status.aimBlend > 0.05f) {
            detail::drawCrosshair(dl, status);
            if (status.inventory && status.inventory->hasBalls()) detail::drawBallSelector(dl, status, device);
        }

        detail::drawNameTags(dl, status);
        detail::drawTint(dl, status);
        detail::drawLevel(dl, status);
        detail::drawVitals(dl, status, device);
        MinimapComponent::draw(dl, status, device);
        detail::drawLocation(dl, status);
        detail::drawTeam(dl, status);
        detail::drawHints(dl, status, device);

        detail::drawWorldPrompts(dl, status, device);
        detail::drawPrompts(dl, status, device);

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
