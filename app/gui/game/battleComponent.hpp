#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "imgui.h"
#include "../../utils/core/crashUtil.hpp"
#include "hudComponent.hpp"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../style/icons/typeIcons.hpp"
#include "../../engine/core/gameEngine.hpp"

// Combate por turnos: pantalla de carga, la escena 3D de fondo (cámara detrás de nuestro pokémon) y encima las barras de vida, los mensajes del
// turno y el menú (Luchar / Pokémon). Todo con el ratón: clic en un movimiento o en un pokémon. El combate en sí vive en Battle.
namespace BattleComponent {
    namespace detail {
        inline constexpr float LOADING_TIME = 2.2f;   // pantalla de carga al entrar
        inline constexpr float LOADING_FADE = 0.5f;   // y lo que tarda en desvanecerse al final
        inline constexpr float MESSAGE_TIME = 1.15f;  // lo que dura cada mensaje (un clic lo salta)
        inline constexpr float BALL_MESSAGE_TIME = 1.75f; // los que sacan o guardan a un pokémon con su pokéball duran lo que dura esa animación
        inline constexpr float FOCUS_TIME = 1.6f;     // lo que dura un mensaje que enseña una tarjeta (la barra baja o sube a la vista)
        inline constexpr float BAR_SPEED = 0.9f;      // fracción de la vida que baja por segundo la barra animada

        enum class Menu { MAIN, MOVES, TARGET, TEAM };

        struct Show { // lo que se dibuja (foto de un mensaje o el estado actual)
            std::string name[Battle::SLOTS];
            int level[Battle::SLOTS] = { 1, 1, 1, 1 };
            int hp[Battle::SLOTS] = { 1, 1, 1, 1 };
            int maxHp[Battle::SLOTS] = { 1, 1, 1, 1 };
            EffectSpec::Status status[Battle::SLOTS] = { EffectSpec::Status::NONE, EffectSpec::Status::NONE, EffectSpec::Status::NONE, EffectSpec::Status::NONE };
            int types[Battle::SLOTS][2] = { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } };
            int stages[Battle::SLOTS][static_cast<int>(EffectSpec::Stat::COUNT)] = {};
            int speciesId[Battle::SLOTS] = { -1, -1, -1, -1 };
            bool onField[Battle::SLOTS] = { false, false, false, false }; // hueco con pokémon que se dibuja
        };

        struct State {
            float time = 0.0f;
            Menu menu = Menu::MAIN;
            bool messageActive = false;
            float messageTime = 0.0f;
            std::string text;
            Show shown;
            float bar[Battle::SLOTS] = { -1.0f, -1.0f, -1.0f, -1.0f }; // PS que muestra cada barra (animados)
            float messageLimit = MESSAGE_TIME;                          // lo que dura el mensaje en curso (más si lleva animación de pokéball)
            int step = 0;                                               // (dobles) a qué hueco del jugador se le está dando la orden
            std::vector<Battle::Action> picked;                         // órdenes ya elegidas este turno
            int pendingMove = -1;                                       // movimiento elegido que espera objetivo
            // Aviso visual del mensaje en curso: sube/baja una estadística o se activa una habilidad.
            int fxSide = -1, fxKind = 0, fxAmount = 0;
            std::string fxText;
            float fxTime = 99.0f;
            bool focus[Battle::SLOTS] = { false, false, false, false }; // tarjetas que se enseñan con el mensaje en curso (daño, estadísticas)
            bool cardFocus = false;
            float overTime = 0.0f; // desde que el combate terminó (para no gastar con el mismo clic el último mensaje)
            int infoMove = -1; // movimiento cuya descripción se muestra (clic derecho sobre su tarjeta)
        };
        inline State& state() { static State s; return s; }

        inline void reset() { state() = State(); }

        inline Show live(const Battle& battle) {
            Show s;
            for (int side = 0; side < Battle::SLOTS; ++side) {
                if (!battle.occupied(side)) continue;
                const Battle::Fighter& f = battle.fighter(side);
                s.onField[side] = true;
                s.name[side] = f.name; s.level[side] = f.level; s.hp[side] = f.hp; s.maxHp[side] = f.maxHp; s.status[side] = f.status; s.speciesId[side] = f.speciesId;
                for (int i = 0; i < 2; ++i) s.types[side][i] = i < static_cast<int>(f.types.size()) ? f.types[static_cast<size_t>(i)] : -1;
                for (int i = 0; i < static_cast<int>(EffectSpec::Stat::COUNT); ++i) s.stages[side][i] = f.stages[static_cast<size_t>(i)];
            }
            return s;
        }

        inline const char* statShort(int stat) {
            static constexpr const char* N[] = { "ATQ", "DEF", "AT.ESP", "DEF.ESP", "VEL", "PREC", "EVA" };
            return N[stat];
        }

        // Cuánto le hace un movimiento al rival, en palabras (vacío si es normal o no hace daño).
        struct Effect { const char* label; ImU32 color; };
        inline Effect effectiveness(const GameData& data, const Move& move, const BattleRules::Fighter& target) {
            if (!move.damaging()) return { "", 0 };
            const float m = data.typeChart.multiplier(move.typeId, target.types);
            if (m <= TypeChart::IMMUNE) return { "NO AFECTA", IM_COL32(0, 0, 0, 255) };           // negro: nulo
            if (m >= 4.0f) return { "HIPEREFICAZ", IM_COL32(255, 150, 30, 255) };               // naranja: x4
            if (m > 1.0f) return { "SUPEREFICAZ", IM_COL32(60, 220, 90, 255) };                 // verde: x2
            if (m <= 0.25f) return { "MUY POCO EFICAZ", IM_COL32(85, 85, 92, 255) };            // gris oscuro: x1/4
            if (m < 1.0f) return { "POCO EFICAZ", IM_COL32(160, 160, 170, 255) };               // gris: x1/2
            return { "", 0 };
        }

        inline ImU32 hpColor(float f) { return f > 0.5f ? IM_COL32(90, 210, 110, 255) : f > 0.2f ? IM_COL32(235, 200, 60, 255) : IM_COL32(230, 70, 70, 255); }

        // Tarjeta de un combatiente: arriba el nombre (izquierda), los tipos (en medio, a la misma altura) y el nivel (derecha); debajo la barra de PS
        // (la nuestra, con los números) y el estado. Los cambios de estadísticas van debajo, pegados al borde, pequeños y sin ocupar sitio en la tarjeta.
        inline void drawBox(ImDrawList* dl, const GameData& data, const ImVec2& pos, const ImVec2& size, const Show& s, int side, float animatedHp) {
            GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y));
            char text[64];
            const float rowY = pos.y + 12.0f, rowMid = rowY + 12.0f;
            std::snprintf(text, sizeof(text), "%s", s.name[side].c_str());
            GuiCards::text(dl, ImVec2(pos.x + 14.0f, rowY), IM_COL32(255, 255, 255, 255), text, 1.15f);
            std::snprintf(text, sizeof(text), "Nv. %d", s.level[side]);
            GuiCards::text(dl, ImVec2(pos.x + size.x - 14.0f - GuiCards::textWidth(text, 1.0f), rowY + 1.0f), IM_COL32(255, 255, 255, 230), text, 1.0f);
            int typeCount = 0;
            for (int i = 0; i < 2; ++i) if (s.types[side][i] >= 0 && data.type(s.types[side][i])) ++typeCount;
            float tx = pos.x + 14.0f + GuiCards::textWidth(s.name[side].c_str(), 1.15f) + 26.0f; // pegados al nombre, con margen
            for (int i = 0; i < 2; ++i) { // tipos: solo el logo, centrados
                const PokemonType* type = s.types[side][i] >= 0 ? data.type(s.types[side][i]) : nullptr;
                if (type) { TypeIcons::draw(dl, ImVec2(tx, rowMid), 12.0f, *type); tx += 30.0f; }
            }
            const float fraction = animatedHp / static_cast<float>((std::max)(1, s.maxHp[side]));
            const float barY = pos.y + 46.0f;
            GuiCards::bar(dl, ImVec2(pos.x + 14.0f, barY), size.x - 28.0f, fraction, hpColor(fraction), 12.0f);
            const char* status = EffectSpec::statusName(s.status[side]);
            if (*status) GuiCards::chip(dl, ImVec2(pos.x + 14.0f, barY + 18.0f), status, s.status[side] == EffectSpec::Status::BURN ? IM_COL32(220, 90, 40, 255) : s.status[side] == EffectSpec::Status::POISON ? IM_COL32(150, 70, 190, 255) : IM_COL32(210, 170, 40, 255), 0.7f);
            if (Battle::sideOf(side) == 0) {
                std::snprintf(text, sizeof(text), "%d / %d", static_cast<int>(std::ceil(animatedHp)), s.maxHp[side]);
                GuiCards::text(dl, ImVec2(pos.x + size.x - 14.0f - GuiCards::textWidth(text, 0.9f), barY + 18.0f), IM_COL32(255, 255, 255, 240), text, 0.9f);
            }
            float sx = pos.x + 12.0f; // estadísticas alteradas ("ATQ +2" verde, "DEF -1" rojo), pegadas al borde inferior
            const float sy = pos.y + size.y - 3.0f;
            for (int i = 0; i < static_cast<int>(EffectSpec::Stat::COUNT); ++i) {
                const int stage = s.stages[side][i];
                if (stage == 0) continue;
                std::snprintf(text, sizeof(text), "%s %+d", statShort(i), stage);
                if (sx + GuiCards::textWidth(text, 0.55f) + 14.0f > pos.x + size.x - 6.0f) break;
                sx += GuiCards::chip(dl, ImVec2(sx, sy), text, stage > 0 ? IM_COL32(40, 150, 80, 255) : IM_COL32(190, 60, 60, 255), 0.55f) + 4.0f;
            }
        }

        // Aviso flotante sobre el cuadro de quien lo recibe: flecha y texto que suben y se desvanecen (verde sube, rojo baja, amarillo habilidad).
        inline void drawFx(ImDrawList* dl, const State& s, const ImVec2& pos, const ImVec2& size) {
            constexpr float DURATION = 1.4f;
            if (s.fxKind == 0 || s.fxTime >= DURATION) return;
            const float t = s.fxTime / DURATION, alpha = t < 0.7f ? 1.0f : (1.0f - t) / 0.3f;
            const int a = static_cast<int>(255.0f * alpha);
            const bool up = s.fxKind == 1, down = s.fxKind == 2;
            const ImU32 color = up ? IM_COL32(70, 220, 110, a) : down ? IM_COL32(240, 80, 80, a) : IM_COL32(250, 210, 70, a);
            std::string label;
            if (s.fxKind == 3) label = s.fxText;
            else {
                std::string stat = s.fxText;
                for (char& c : stat) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                label = stat + (up ? " SUBE" : " BAJA");
                if (std::abs(s.fxAmount) > 1) label += up ? " MUCHO" : " MUCHO";
            }
            const float rise = 34.0f * t, cx = pos.x + size.x * 0.5f, top = pos.y - 8.0f - rise;
            const float w = GuiCards::textWidth(label.c_str(), 1.1f);
            dl->AddRectFilled(ImVec2(cx - w * 0.5f - 34.0f, top - 22.0f), ImVec2(cx + w * 0.5f + 12.0f, top + 8.0f), IM_COL32(15, 15, 25, static_cast<int>(210.0f * alpha)), 14.0f);
            GuiCards::text(dl, ImVec2(cx - w * 0.5f, top - 18.0f), color, label.c_str(), 1.1f);
            const float ax = cx - w * 0.5f - 18.0f, ay = top - 7.0f; // flecha (triángulo)
            if (up) dl->AddTriangleFilled(ImVec2(ax, ay - 9.0f), ImVec2(ax - 8.0f, ay + 6.0f), ImVec2(ax + 8.0f, ay + 6.0f), color);
            else if (down) dl->AddTriangleFilled(ImVec2(ax, ay + 9.0f), ImVec2(ax - 8.0f, ay - 6.0f), ImVec2(ax + 8.0f, ay - 6.0f), color);
            else dl->AddCircleFilled(ImVec2(ax, ay), 7.0f, color);
        }

        // Símbolo de la clase del movimiento: físico (estallido naranja), especial (círculos azules concéntricos), estado (remolino gris).
        inline void classIcon(ImDrawList* dl, const ImVec2& c, float r, MoveClass::Kind kind) {
            if (kind == MoveClass::Kind::PHYSICAL) {
                dl->AddCircleFilled(c, r, IM_COL32(210, 90, 40, 255), 24);
                for (int i = 0; i < 8; ++i) {
                    const float a = 0.785398f * static_cast<float>(i), b = a + 0.392699f;
                    dl->AddTriangleFilled(ImVec2(c.x + std::cos(a) * r * 0.25f, c.y + std::sin(a) * r * 0.25f), ImVec2(c.x + std::cos(a - 0.2f) * r * 0.85f, c.y + std::sin(a - 0.2f) * r * 0.85f), ImVec2(c.x + std::cos(b) * r * 0.55f, c.y + std::sin(b) * r * 0.55f), IM_COL32(255, 235, 200, 255));
                }
            } else if (kind == MoveClass::Kind::SPECIAL) {
                dl->AddCircleFilled(c, r, IM_COL32(60, 100, 210, 255), 24);
                dl->AddCircle(c, r * 0.68f, IM_COL32(220, 235, 255, 255), 24, 2.0f);
                dl->AddCircleFilled(c, r * 0.3f, IM_COL32(220, 235, 255, 255), 16);
            } else {
                dl->AddCircleFilled(c, r, IM_COL32(110, 115, 130, 255), 24);
                dl->PathArcTo(c, r * 0.62f, 0.3f, 4.6f, 20);
                dl->PathStroke(IM_COL32(235, 235, 245, 255), 0, 2.2f);
                dl->PathArcTo(c, r * 0.3f, 3.4f, 7.2f, 14);
                dl->PathStroke(IM_COL32(235, 235, 245, 255), 0, 2.0f);
            }
            dl->AddCircle(c, r, IM_COL32(0, 0, 0, 150), 24, 1.5f);
        }

        inline std::string typeName(const GameData& data, int typeId) {
            const PokemonType* type = data.type(typeId);
            if (!type) return "Normal";
            std::string name = type->name;
            if (!name.empty() && static_cast<unsigned char>(name[0]) < 128) name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            return name;
        }
    }

    // Se llama al entrar en el combate.
    inline void begin() { detail::reset(); }

    // Dibuja la pantalla y avanza el combate. Devuelve a PLAYING al terminar.
    inline void render(GameState& state, GameState& backState, GameEngine& engine) {
        CRASH_SCOPE();
        using namespace detail;
        State& s = detail::state();
        const float dt = (std::min)(ImGui::GetIO().DeltaTime, 0.1f);
        const std::shared_ptr<Battle> battle = engine.battle();
        if (!battle) { state = GameState::PLAYING; return; }
        const GameData& data = engine.data();
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        s.time += dt;
        CrashUtil::detail("combate: avance (t=%.2f)", s.time);
        engine.battleAdvance(dt);
        CrashUtil::detail("combate: dibujo (mensaje=%.0f, listo=%.0f)", battle->hasMessage() ? 1.0 : 0.0, battle->ready() ? 1.0 : 0.0);

        // Carga inicial: negro con el título y quién nos reta; el combate no avanza hasta que se desvanece.
        if (s.time < LOADING_TIME) {
            const float fade = (std::min)(1.0f, (LOADING_TIME - s.time) / LOADING_FADE);
            const std::string title = "¡Combate!", detail = battle->foeName() + " quiere combatir";
            HudComponent::detail::drawLoading(dl, fade, &title, &detail);
            return;
        }

        // Mensajes del turno, uno a uno (clic o espacio para pasar).
        const bool skip = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_Enter);
        if (battle->hasMessage()) {
            if (!s.messageActive) {
                s.messageActive = true;
                s.messageTime = 0.0f;
                const Battle::Message& m = battle->front();
                s.text = m.text;
                s.messageLimit = m.animKind == 1 || m.animKind == 2 ? BALL_MESSAGE_TIME : MESSAGE_TIME;
                s.cardFocus = false; // daño o cambio de estadística: en vez del texto se enseña la tarjeta del pokémon afectado
                for (int slot = 0; slot < Battle::SLOTS; ++slot) {
                    const bool hurt = m.hit && m.target == slot;
                    const bool staged = (m.fxKind == 1 || m.fxKind == 2) && m.fxSide == slot;
                    const bool changed = m.onField[slot] && s.shown.onField[slot] && m.hp[slot] != s.shown.hp[slot];
                    s.focus[slot] = hurt || staged || changed;
                    s.cardFocus = s.cardFocus || s.focus[slot];
                }
                if (s.cardFocus) s.messageLimit = FOCUS_TIME;
                for (int side = 0; side < Battle::SLOTS; ++side) {
                    s.shown.name[side] = m.name[side]; s.shown.level[side] = m.level[side]; s.shown.hp[side] = m.hp[side]; s.shown.maxHp[side] = m.maxHp[side]; s.shown.status[side] = m.status[side]; s.shown.speciesId[side] = m.speciesId[side]; s.shown.onField[side] = m.onField[side];
                    for (int i = 0; i < 2; ++i) s.shown.types[side][i] = m.types[side][i];
                    for (int i = 0; i < static_cast<int>(EffectSpec::Stat::COUNT); ++i) s.shown.stages[side][i] = m.stages[side][i];
                }
                s.fxSide = m.fxSide; s.fxKind = m.fxKind; s.fxAmount = m.fxAmount; s.fxText = m.fxText; s.fxTime = 0.0f;
                CrashUtil::detail("combate: animación %.0f en hueco %.0f", static_cast<double>(m.animKind), static_cast<double>(m.animSide));
                engine.battlePlayFx(m);
            }
            s.messageTime += dt;
            if (s.messageTime >= s.messageLimit || (skip && s.messageTime > 0.15f)) { battle->pop(); s.messageActive = false; }
        } else {
            s.messageActive = false;
            s.shown = live(*battle);
        }

        // Barras de PS: se desplazan hacia el valor mostrado.
        for (int side = 0; side < Battle::SLOTS; ++side) {
            const float target = static_cast<float>(s.shown.hp[side]);
            if (s.bar[side] < 0.0f) s.bar[side] = target;
            const float step = BAR_SPEED * static_cast<float>(s.shown.maxHp[side]) * dt;
            s.bar[side] = s.bar[side] < target ? (std::min)(target, s.bar[side] + step) : (std::max)(target, s.bar[side] - step);
        }
        s.fxTime += dt;

        // Cámara: se puede girar alrededor de la zona de combate (A/D o flechas, o arrastrando con el botón derecho) y subirla o bajarla (W/S o arrastrando).
        {
            const ImGuiIO& io = ImGui::GetIO();
            float yawDelta = 0.0f, tiltDelta = 0.0f;
            if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) yawDelta += 1.2f * dt;
            if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) yawDelta -= 1.2f * dt; // D lleva la cámara a la derecha
            if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) tiltDelta += 0.6f * dt;
            if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) tiltDelta -= 0.6f * dt;
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4.0f)) { yawDelta -= io.MouseDelta.x * 0.006f; tiltDelta += io.MouseDelta.y * 0.004f; }
            if (yawDelta != 0.0f || tiltDelta != 0.0f) engine.battleOrbit(yawDelta, tiltDelta);
            GuiCards::text(dl, ImVec2(screen.x * 0.5f - GuiCards::textWidth("A/D o clic derecho + arrastrar: girar cámara", 0.8f) * 0.5f, 16.0f), IM_COL32(255, 255, 255, 150), "A/D o clic derecho + arrastrar: girar cámara", 0.8f);
        }

        CRASH_AT();
        // Disposición (estilo Pokémon Champions): al elegir, nuestras tarjetas abajo a la izquierda, las del rival arriba a la derecha y los controles en vertical a la derecha.
        // Al jugarse el turno desaparece todo menos el texto (translúcido, abajo); cuando un pokémon recibe daño o le cambia una estadística, el texto cede su sitio a la tarjeta de ese pokémon.
        const bool doubles = battle->doubles();
        const bool choosing = battle->ready() && battle->phase() != Battle::Phase::OVER;
        const bool over = battle->phase() == Battle::Phase::OVER && !battle->hasMessage();
        s.overTime = over ? s.overTime + dt : 0.0f;
        const bool firstLeft = engine.battleFirstOnLeft();
        const float cardWidth = doubles ? 330.0f : 400.0f, cardHeight = 92.0f, cardGap = 12.0f, margin = 30.0f;
        const float menuW = 460.0f, menuRight = screen.x - margin, menuLeft = menuRight - menuW, menuBottom = screen.y - 28.0f, menuTop = menuBottom - 372.0f;
        const auto column = [&](int slot) { return !doubles ? 0 : ((Battle::posOf(slot) == 0) == firstLeft ? 0 : 1); }; // la tarjeta de la izquierda es la del pokémon que se ve a la izquierda
        const auto cardPos = [&](int slot) {
            const int col = column(slot);
            if (Battle::sideOf(slot) == 1) {
                const float x = doubles ? screen.x - margin - (2.0f - static_cast<float>(col)) * cardWidth - (1.0f - static_cast<float>(col)) * cardGap : screen.x - margin - cardWidth;
                return ImVec2(x, 40.0f);
            }
            return ImVec2(margin + static_cast<float>(col) * (cardWidth + cardGap), screen.y - 28.0f - cardHeight);
        };
        const ImVec2 cardSize(cardWidth, cardHeight);
        bool animating[Battle::SLOTS] = {};
        for (int slot = 0; slot < Battle::SLOTS; ++slot) animating[slot] = std::abs(s.bar[slot] - static_cast<float>(s.shown.hp[slot])) > 0.5f;
        bool showAnimating = false;
        for (int slot = 0; slot < Battle::SLOTS; ++slot) showAnimating = showAnimating || (animating[slot] && s.shown.onField[slot]);
        const bool cardsOnly = (s.messageActive && s.cardFocus) || (!s.messageActive && !choosing && !over && showAnimating);
        for (int slot = 0; slot < Battle::SLOTS; ++slot) {
            if (!s.shown.onField[slot]) continue;
            const bool visible = choosing ? true : cardsOnly ? (s.messageActive ? s.focus[slot] : animating[slot]) : false;
            if (!visible) continue;
            const ImVec2 pos = cardPos(slot);
            drawBox(dl, data, pos, cardSize, s.shown, slot, s.bar[slot]);
            if (s.fxSide == slot && s.messageActive) drawFx(dl, s, ImVec2(pos.x, pos.y - 4.0f), cardSize);
        }

        CRASH_AT();
        // Texto del turno: translúcido, abajo, legible sobre la escena. Desaparece cuando se enseña una tarjeta.
        std::string line = s.messageActive ? s.text : std::string();
        if (!s.messageActive && over) line = battle->outcome() == Battle::Outcome::WON ? "¡Has ganado el combate!" : "Has perdido el combate...";
        if (!line.empty() && !cardsOnly) {
            const float boxW = (std::min)(screen.x - 2.0f * margin, 1100.0f), boxH = 92.0f, boxX = (screen.x - boxW) * 0.5f, boxY = screen.y - 28.0f - boxH;
            dl->AddRectFilled(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), IM_COL32(8, 10, 20, 120), 16.0f);
            dl->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), IM_COL32(255, 255, 255, 60), 16.0f, 0, 1.5f);
            ImGui::SetWindowFontScale(1.3f);
            ImGui::PushTextWrapPos(boxX + boxW - 28.0f - ImGui::GetWindowPos().x);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 230));
            ImGui::SetCursorScreenPos(ImVec2(boxX + 29.5f, boxY + 22.5f));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            ImGui::SetCursorScreenPos(ImVec2(boxX + 28.0f, boxY + 21.0f));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
            ImGui::SetWindowFontScale(1.0f);
            if (battle->hasMessage()) GuiCards::text(dl, ImVec2(boxX + boxW - 130.0f, boxY + boxH - 24.0f), IM_COL32(255, 255, 255, 170), "Clic: continuar", 0.8f);
        }

        CRASH_AT();
        if (!choosing) { s.step = 0; s.picked.clear(); s.pendingMove = -1; }
        if (choosing) {
            const bool forced = battle->phase() == Battle::Phase::FORCED_SWITCH;
            const std::vector<int> mineSlots = battle->playerSlots();
            if (mineSlots.empty() && !forced) { ImGui::Dummy(ImVec2(0.0f, 0.0f)); return; }
            if (s.step >= static_cast<int>(mineSlots.size())) { s.step = 0; s.picked.clear(); }
            const int slot = forced ? battle->pendingSlot() : mineSlots[static_cast<size_t>(s.step)]; // hueco al que se le da la orden
            if (slot < 0 || slot >= Battle::SLOTS) { CrashUtil::note("combate: hueco inválido %.0f (fase %.0f)", static_cast<double>(slot), static_cast<double>(battle->phase())); return; }
            // Orden elegida para ese hueco: cuando están todas (una en individuales, dos en dobles) se juega el turno.
            bool committed = false; // tras dar una orden el combate puede haber cambiado (rivales caídos, huecos vacíos): se deja de dibujar este fotograma
            const auto commit = [&](const Battle::Action& action) {
                committed = true;
                s.picked.push_back(action);
                ++s.step;
                s.menu = Menu::MAIN;
                s.pendingMove = -1;
                if (s.step >= static_cast<int>(mineSlots.size())) {
                    if (!battle->submit(s.picked)) std::fprintf(stderr, "[combate] órdenes rechazadas (%zu)\n", s.picked.size());
                    s.picked.clear();
                    s.step = 0;
                }
            };
            int reserved = -1; // pokémon que ya va a sacar el otro hueco
            for (const Battle::Action& a : s.picked) if (a.kind == Battle::Action::Kind::SWITCH) reserved = a.index;
            const std::vector<int> foes = battle->foeSlots();
            const int firstFoe = foes.empty() ? -1 : foes.front();
            if (forced) s.menu = Menu::TEAM;
            if (s.menu != Menu::MOVES) s.infoMove = -1;
            CRASH_AT();
            const float backY = menuBottom - 30.0f;
            if (s.menu == Menu::MAIN) {
                const float h = 66.0f, gap = 12.0f;
                const float top = menuBottom - 3.0f * h - 2.0f * gap;
                ImGui::SetCursorScreenPos(ImVec2(menuLeft, top));
                if (GuiCards::button("##fight", "LUCHAR", ImVec2(menuW, h), true, IM_COL32(200, 70, 50, 255))) s.menu = Menu::MOVES;
                ImGui::SetCursorScreenPos(ImVec2(menuLeft, top + h + gap));
                if (GuiCards::button("##team", "POKÉMON", ImVec2(menuW, h), true, IM_COL32(60, 150, 90, 255))) s.menu = Menu::TEAM;
                ImGui::SetCursorScreenPos(ImVec2(menuLeft, top + 2.0f * (h + gap)));
                if (GuiCards::button("##types", "TABLA DE TIPOS", ImVec2(menuW, h), true, IM_COL32(70, 110, 190, 255))) { backState = GameState::BATTLE; state = GameState::TYPES; }
                if (s.step > 0) { // dobles: volver a elegir lo del primer pokémon
                    ImGui::SetCursorScreenPos(ImVec2(menuLeft - 140.0f, menuBottom - 28.0f));
                    if (GuiCards::button("##undo", "< ANTERIOR", ImVec2(126.0f, 28.0f), true, IM_COL32(70, 70, 92, 255))) { s.picked.pop_back(); --s.step; }
                }
            } else if (s.menu == Menu::MOVES) {
                const Battle::Fighter& me = battle->fighter(slot);
                const bool struggle = battle->mustStruggle(slot);
                const float gap = 14.0f, w = menuW, h = 66.0f;
                const float top = menuBottom - 36.0f - 4.0f * h - 3.0f * gap;
                if (struggle) {
                    ImGui::SetCursorScreenPos(ImVec2(menuLeft, top));
                    if (GuiCards::button("##struggle", "FORCEJEO (sin PP)", ImVec2(w, h), true, IM_COL32(150, 70, 70, 255))) {
                        if (battle->needsTarget(slot, 0)) { s.pendingMove = 0; s.menu = Menu::TARGET; }
                        else { Battle::Action a; a.index = 0; commit(a); }
                    }
                }
                bool infoClicked = false;
                for (int i = 0; i < MoveRules::MAX_KNOWN && !struggle && !committed; ++i) {
                    const float x = menuLeft, y = top + static_cast<float>(i) * (h + gap);
                    const Move* move = data.move(me.moves[i]);
                    ImGui::SetCursorScreenPos(ImVec2(x, y));
                    ImGui::PushID(i);
                    const bool usable = move && battle->canUse(slot, i);
                    const bool clicked = ImGui::InvisibleButton("##move", ImVec2(w, h)) && usable;
                    const bool hover = ImGui::IsItemHovered();
                    if (hover && move && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::GetIO().MouseDragMaxDistanceSqr[1] < 16.0f) { s.infoMove = s.infoMove == i ? -1 : i; infoClicked = true; }
                    ImGui::PopID();
                    const ImVec2 a(x, y), b(x + w, y + h);
                    dl->AddRectFilled(a, b, !move ? IM_COL32(40, 40, 50, 160) : !usable ? IM_COL32(50, 50, 60, 230) : hover ? IM_COL32(52, 55, 78, 255) : IM_COL32(34, 36, 52, 245), 12.0f);
                    if (!move) { GuiCards::centeredText(dl, ImVec2(x + w * 0.5f, y + h * 0.5f), GuiStyle::MUTED, "—", 1.0f); continue; }
                    // Borde: el normal de la tarjeta; cambia de color según lo eficaz que sea contra el rival (en dobles, contra el objetivo: se ve al elegirlo).
                    const Effect effect = foes.size() == 1 && firstFoe >= 0 && battle->occupied(firstFoe) ? effectiveness(data, *move, battle->fighter(firstFoe)) : Effect{ "", 0 };
                    if (*effect.label) dl->AddRect(a, b, effect.color, 12.0f, 0, 4.0f);
                    else dl->AddRect(a, b, hover && usable ? GuiCards::SELECT : IM_COL32(120, 125, 150, 255), 12.0f, 0, hover && usable ? 2.5f : 1.5f);
                    const ImU32 ink = usable ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 120);
                    // Todo en una misma fila, a la altura del centro de la tarjeta: clase y tipo a la izquierda, nombre en medio, números a la derecha.
                    const float cy = y + h * 0.5f;
                    detail::classIcon(dl, ImVec2(x + 26.0f, cy), 14.0f, move->kind);
                    if (const PokemonType* type = data.type(move->typeId)) TypeIcons::draw(dl, ImVec2(x + 62.0f, cy), 11.0f, *type);
                    char stats[64];
                    std::snprintf(stats, sizeof(stats), "POT %s   PREC %s   PP %d/%d", move->power > 0 ? std::to_string(move->power).c_str() : "-", move->accuracy > 0 ? std::to_string(move->accuracy).c_str() : "-", me.pp[i], move->pp);
                    const float statsScale = 0.72f, statsWidth = GuiCards::textWidth(stats, statsScale);
                    const float leftEnd = x + 84.0f, statsLeft = x + w - 12.0f - statsWidth;
                    float nameScale = 1.05f, nameWidth = GuiCards::textWidth(move->name.c_str(), nameScale);
                    if (nameWidth > statsLeft - leftEnd - 12.0f) { nameScale = (std::max)(0.6f, nameScale * (statsLeft - leftEnd - 12.0f) / nameWidth); nameWidth = GuiCards::textWidth(move->name.c_str(), nameScale); }
                    const float nameCenter = (std::clamp)(x + w * 0.5f, leftEnd + nameWidth * 0.5f + 6.0f, (std::max)(leftEnd + nameWidth * 0.5f + 6.0f, statsLeft - nameWidth * 0.5f - 6.0f)); // centrado en la tarjeta si cabe
                    GuiCards::centeredText(dl, ImVec2(nameCenter, cy), ink, move->name.c_str(), nameScale);
                    GuiCards::centeredText(dl, ImVec2(statsLeft + statsWidth * 0.5f, cy), usable ? IM_COL32(255, 255, 255, 230) : IM_COL32(255, 255, 255, 110), stats, statsScale);
                    // Eficacia: texto pequeño centrado sobre el borde superior (nada si el daño es normal), con su propio contorno.
                    if (*effect.label) {
                        const float labelScale = 0.58f, tw = GuiCards::textWidth(effect.label, labelScale) + 14.0f, th = ImGui::GetFontSize() * labelScale + 4.0f;
                        const ImVec2 lo(x + w * 0.5f - tw * 0.5f, y - th * 0.5f), hi(x + w * 0.5f + tw * 0.5f, y + th * 0.5f);
                        const ImU32 edge = effect.color == IM_COL32(0, 0, 0, 255) ? IM_COL32(120, 120, 130, 255) : effect.color;
                        dl->AddRectFilled(lo, hi, IM_COL32(16, 16, 22, 255), th * 0.5f);
                        dl->AddRect(lo, hi, edge, th * 0.5f, 0, 2.0f);
                        GuiCards::centeredText(dl, ImVec2((lo.x + hi.x) * 0.5f, y), IM_COL32(255, 255, 255, 255), effect.label, labelScale);
                    }
                    if (clicked) {
                        if (battle->needsTarget(slot, i)) { s.pendingMove = i; s.menu = Menu::TARGET; }
                        else { Battle::Action act; act.index = i; commit(act); }
                    }
                }
                if (!infoClicked && s.infoMove >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) s.infoMove = -1;
                if (s.infoMove >= 0 && s.infoMove < MoveRules::MAX_KNOWN) { // descripción y efecto: solo con clic derecho en la tarjeta (a la izquierda de los controles)
                    if (const Move* move = data.move(me.moves[s.infoMove])) {
                        const float pw = 440.0f, ph = 96.0f, px = menuLeft - 16.0f - pw, py = menuBottom - ph;
                        GuiCards::panel(dl, ImVec2(px, py), ImVec2(px + pw, py + ph));
                        GuiCards::text(dl, ImVec2(px + 14.0f, py + 8.0f), IM_COL32(255, 255, 255, 255), move->name.c_str(), 1.0f);
                        const Effect effect = firstFoe >= 0 && battle->occupied(firstFoe) && foes.size() == 1 ? effectiveness(data, *move, battle->fighter(firstFoe)) : Effect{ "", 0 };
                        if (*effect.label) GuiCards::chip(dl, ImVec2(px + pw - 14.0f - GuiCards::textWidth(effect.label, 0.7f) - 16.0f, py + 8.0f), effect.label, effect.color, 0.7f);
                        ImGui::SetCursorScreenPos(ImVec2(px + 14.0f, py + 34.0f));
                        ImGui::PushTextWrapPos(px + pw - 14.0f - ImGui::GetWindowPos().x);
                        ImGui::TextUnformatted(move->description.c_str());
                        ImGui::PopTextWrapPos();
                    }
                }
                ImGui::SetCursorScreenPos(ImVec2(menuLeft, backY));
                if (GuiCards::button("##back", "< ATRÁS", ImVec2(110.0f, 28.0f), true, IM_COL32(70, 70, 92, 255))) s.menu = Menu::MAIN;
                if (!struggle) GuiCards::text(dl, ImVec2(menuLeft + 126.0f, backY + 4.0f), IM_COL32(255, 255, 255, 190), "Clic derecho: descripción", 0.8f);
            } else if (s.menu == Menu::TARGET) { // dobles: a cuál de los dos rivales se ataca
                const Battle::Fighter& me = battle->fighter(slot);
                const Move* move = s.pendingMove >= 0 ? data.move(me.moves[s.pendingMove]) : nullptr;
                // Dos filas como en el campo: los rivales arriba y los nuestros abajo (separadas por una línea discontinua); dentro de cada fila, cada botón
                // está a la izquierda o a la derecha según el pokémon al que corresponde.
                const std::vector<int> choices = battle->targetChoices(slot, s.pendingMove);
                const float h = 96.0f, halfW = (menuW - 12.0f) * 0.5f;
                const float rowFoe = menuBottom - 36.0f - 2.0f * h - 26.0f, rowOwn = menuBottom - 36.0f - h;
                for (float x = menuLeft; x < menuRight; x += 16.0f) dl->AddLine(ImVec2(x, rowFoe + h + 13.0f), ImVec2((std::min)(x + 8.0f, menuRight), rowFoe + h + 13.0f), IM_COL32(255, 255, 255, 150), 2.0f);
                for (size_t k = 0; k < choices.size() && !committed; ++k) {
                    const int who = choices[k];
                    const Battle::Fighter& other = battle->fighter(who);
                    const bool hostile = Battle::sideOf(who) != Battle::sideOf(slot);
                    const int col = choices.size() > 1 || !hostile ? column(who) : 0;
                    const float x = menuLeft + (doubles ? static_cast<float>(col) * (halfW + 12.0f) : 0.0f), w = doubles ? halfW : menuW;
                    const float y = hostile ? rowFoe : rowOwn;
                    ImGui::SetCursorScreenPos(ImVec2(x, y));
                    ImGui::PushID(200 + static_cast<int>(k));
                    char label[96];
                    std::snprintf(label, sizeof(label), "%s Nv.%d%s", other.name.c_str(), other.level, hostile ? "" : who == slot ? " (tú)" : " (aliado)");
                    const Effect effect = move && hostile ? effectiveness(data, *move, other) : Effect{ "", 0 };
                    const bool clicked = GuiCards::button("##target", label, ImVec2(w, h), true, hostile ? IM_COL32(200, 70, 50, 255) : IM_COL32(60, 150, 90, 255));
                    ImGui::PopID();
                    if (*effect.label) GuiCards::chip(dl, ImVec2(x + 10.0f, y + h - 26.0f), effect.label, effect.color, 0.7f);
                    if (clicked) { Battle::Action act; act.index = s.pendingMove; act.target = who; commit(act); }
                }
                ImGui::SetCursorScreenPos(ImVec2(menuLeft, backY));
                if (GuiCards::button("##back", "< ATRÁS", ImVec2(110.0f, 28.0f), true, IM_COL32(70, 70, 92, 255))) s.menu = Menu::MOVES;
            } else { // equipo: una columna en la zona de los controles
                const float gap = 6.0f, w = menuW, rowH = 46.0f;
                const int n = static_cast<int>(battle->team(0).size());
                const float top = menuBottom - 36.0f - static_cast<float>(n) * (rowH + gap) + gap;
                for (int i = 0; i < n && !committed; ++i) {
                    const Battle::Fighter& f = battle->team(0)[static_cast<size_t>(i)];
                    const float x = menuLeft, y = top + static_cast<float>(i) * (rowH + gap);
                    ImGui::SetCursorScreenPos(ImVec2(x, y));
                    ImGui::PushID(100 + i);
                    const bool ok = battle->canSwitchTo(i, reserved);
                    const bool clicked = ImGui::InvisibleButton("##member", ImVec2(w, rowH)) && ok;
                    const bool hover = ImGui::IsItemHovered() && ok;
                    ImGui::PopID();
                    const bool onField = i == battle->active(0) || i == battle->active(1);
                    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + rowH), hover ? IM_COL32(70, 70, 96, 255) : onField ? IM_COL32(50, 80, 60, 230) : GuiStyle::SURFACE, 12.0f);
                    ItemIcon::creature(dl, ImVec2(x + 26.0f, y + rowH * 0.5f), 16.0f, f.speciesId);
                    char text[64];
                    std::snprintf(text, sizeof(text), "%s  Nv. %d", f.name.c_str(), f.level);
                    GuiCards::text(dl, ImVec2(x + 50.0f, y + 5.0f), IM_COL32(255, 255, 255, f.fainted() ? 130 : 255), text, 0.85f);
                    float tx = x + w - 12.0f - 24.0f * static_cast<float>(f.types.size()); // tipos: logos a la derecha
                    for (const int typeId : f.types) if (const PokemonType* type = data.type(typeId)) { TypeIcons::draw(dl, ImVec2(tx + 10.0f, y + 15.0f), 10.0f, *type); tx += 24.0f; }
                    GuiCards::bar(dl, ImVec2(x + 50.0f, y + 29.0f), w - 130.0f, f.hpFraction(), hpColor(f.hpFraction()), 7.0f);
                    std::snprintf(text, sizeof(text), f.fainted() ? "Debilitado" : "%d/%d", f.hp, f.maxHp);
                    GuiCards::text(dl, ImVec2(x + w - 74.0f, y + 26.0f), IM_COL32(255, 255, 255, 210), text, 0.7f);
                    if (clicked) {
                        if (forced) { battle->forceSwitch(i); s.menu = Menu::MAIN; }
                        else { Battle::Action a; a.kind = Battle::Action::Kind::SWITCH; a.index = i; commit(a); }
                    }
                }
                if (!forced) {
                    ImGui::SetCursorScreenPos(ImVec2(menuLeft, backY));
                    if (GuiCards::button("##back", "< ATRÁS", ImVec2(110.0f, 28.0f), true, IM_COL32(70, 70, 92, 255))) s.menu = Menu::MAIN;
                }
            }
            if (committed) { ImGui::Dummy(ImVec2(0.0f, 0.0f)); return; }
        } else if (over && s.overTime > 0.35f && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { // un clic en cualquier parte y se sale
            engine.battleFinish();
            state = GameState::PLAYING;
            return;
        }
        ImGui::Dummy(ImVec2(0.0f, 0.0f)); // los SetCursorScreenPos de arriba necesitan un elemento al final
    }
}
