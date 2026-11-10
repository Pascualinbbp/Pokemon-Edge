#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include "imgui.h"
#include "hudComponent.hpp"
#include "../gameState.hpp"
#include "../style/guiCards.hpp"
#include "../style/guiStyle.hpp"
#include "../style/icons/itemIcon.hpp"
#include "../../engine/core/gameEngine.hpp"

// Combate por turnos: pantalla de carga, la escena 3D de fondo (cámara detrás de nuestro pokémon) y encima las barras de vida, los mensajes del
// turno y el menú (Luchar / Pokémon). Todo con el ratón: clic en un movimiento o en un pokémon. El combate en sí vive en Battle.
namespace BattleComponent {
    namespace detail {
        inline constexpr float LOADING_TIME = 2.2f;   // pantalla de carga al entrar
        inline constexpr float LOADING_FADE = 0.5f;   // y lo que tarda en desvanecerse al final
        inline constexpr float MESSAGE_TIME = 1.15f;  // lo que dura cada mensaje (un clic lo salta)
        inline constexpr float BAR_SPEED = 0.9f;      // fracción de la vida que baja por segundo la barra animada

        enum class Menu { MAIN, MOVES, TEAM };

        struct Show { // lo que se dibuja (foto de un mensaje o el estado actual)
            std::string name[2];
            int level[2] = { 1, 1 };
            int hp[2] = { 1, 1 };
            int maxHp[2] = { 1, 1 };
            EffectSpec::Status status[2] = { EffectSpec::Status::NONE, EffectSpec::Status::NONE };
            int types[2][2] = { { -1, -1 }, { -1, -1 } };
            int stages[2][static_cast<int>(EffectSpec::Stat::COUNT)] = {};
        };

        struct State {
            float time = 0.0f;
            Menu menu = Menu::MAIN;
            bool messageActive = false;
            float messageTime = 0.0f;
            std::string text;
            Show shown;
            float bar[2] = { -1.0f, -1.0f }; // PS que muestra cada barra (animados)
            // Aviso visual del mensaje en curso: sube/baja una estadística o se activa una habilidad.
            int fxSide = -1, fxKind = 0, fxAmount = 0;
            std::string fxText;
            float fxTime = 99.0f;
        };
        inline State& state() { static State s; return s; }

        inline void reset() { state() = State(); }

        inline Show live(const Battle& battle) {
            Show s;
            for (int side = 0; side < 2; ++side) {
                const Battle::Fighter& f = battle.fighter(side);
                s.name[side] = f.name; s.level[side] = f.level; s.hp[side] = f.hp; s.maxHp[side] = f.maxHp; s.status[side] = f.status;
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
            if (m <= TypeChart::IMMUNE) return { "NO AFECTA", IM_COL32(70, 70, 85, 255) };
            if (m >= 4.0f) return { "HIPEREFICAZ", IM_COL32(150, 60, 220, 255) };
            if (m > 1.0f) return { "SUPEREFICAZ", IM_COL32(40, 160, 80, 255) };
            if (m <= 0.25f) return { "MUY POCO EFICAZ", IM_COL32(170, 70, 60, 255) };
            if (m < 1.0f) return { "POCO EFICAZ", IM_COL32(200, 120, 40, 255) };
            return { "", 0 };
        }

        inline ImU32 hpColor(float f) { return f > 0.5f ? IM_COL32(90, 210, 110, 255) : f > 0.2f ? IM_COL32(235, 200, 60, 255) : IM_COL32(230, 70, 70, 255); }

        // Cuadro de un combatiente: nombre, nivel, tipos, estado, cambios de estadísticas y barra de PS (la del jugador también con los números).
        inline void drawBox(ImDrawList* dl, const GameData& data, const ImVec2& pos, const ImVec2& size, const Show& s, int side, float animatedHp) {
            GuiCards::panel(dl, pos, ImVec2(pos.x + size.x, pos.y + size.y));
            char text[64];
            std::snprintf(text, sizeof(text), "%s", s.name[side].c_str());
            GuiCards::text(dl, ImVec2(pos.x + 14.0f, pos.y + 8.0f), IM_COL32(255, 255, 255, 255), text, 1.2f);
            std::snprintf(text, sizeof(text), "Nv. %d", s.level[side]);
            GuiCards::text(dl, ImVec2(pos.x + size.x - 14.0f - GuiCards::textWidth(text, 1.0f), pos.y + 10.0f), IM_COL32(255, 255, 255, 230), text, 1.0f);
            float x = pos.x + 14.0f;
            const float y = pos.y + 36.0f;
            for (int i = 0; i < 2; ++i) { // tipos
                const PokemonType* type = s.types[side][i] >= 0 ? data.type(s.types[side][i]) : nullptr;
                if (type) x += GuiCards::typeChip(dl, ImVec2(x, y), *type) + 6.0f;
            }
            const char* status = EffectSpec::statusName(s.status[side]);
            if (*status) x += GuiCards::chip(dl, ImVec2(x, y), status, s.status[side] == EffectSpec::Status::BURN ? IM_COL32(220, 90, 40, 255) : s.status[side] == EffectSpec::Status::POISON ? IM_COL32(150, 70, 190, 255) : IM_COL32(210, 170, 40, 255), 0.75f) + 6.0f;
            float sx = pos.x + 14.0f; // estadísticas alteradas: "ATQ +2" en verde, "DEF -1" en rojo
            for (int i = 0; i < static_cast<int>(EffectSpec::Stat::COUNT); ++i) {
                const int stage = s.stages[side][i];
                if (stage == 0) continue;
                std::snprintf(text, sizeof(text), "%s %+d", statShort(i), stage);
                if (sx + GuiCards::textWidth(text, 0.7f) + 16.0f > pos.x + size.x - 8.0f) break;
                sx += GuiCards::chip(dl, ImVec2(sx, pos.y + 62.0f), text, stage > 0 ? IM_COL32(40, 150, 80, 255) : IM_COL32(190, 60, 60, 255), 0.7f) + 5.0f;
            }
            const float fraction = animatedHp / static_cast<float>((std::max)(1, s.maxHp[side]));
            GuiCards::bar(dl, ImVec2(pos.x + 14.0f, pos.y + size.y - 24.0f), size.x - 28.0f, fraction, hpColor(fraction), 12.0f);
            if (side == 0) {
                std::snprintf(text, sizeof(text), "%d / %d", static_cast<int>(std::ceil(animatedHp)), s.maxHp[side]);
                GuiCards::text(dl, ImVec2(pos.x + size.x - 14.0f - GuiCards::textWidth(text, 0.85f), y + 2.0f), IM_COL32(255, 255, 255, 235), text, 0.85f);
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
    inline void render(GameState& state, GameEngine& engine) {
        using namespace detail;
        State& s = detail::state();
        const float dt = (std::min)(ImGui::GetIO().DeltaTime, 0.1f);
        const std::shared_ptr<Battle> battle = engine.battle();
        if (!battle) { state = GameState::PLAYING; return; }
        const GameData& data = engine.data();
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        s.time += dt;
        engine.battleAdvance(dt);

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
                for (int side = 0; side < 2; ++side) {
                    s.shown.name[side] = m.name[side]; s.shown.level[side] = m.level[side]; s.shown.hp[side] = m.hp[side]; s.shown.maxHp[side] = m.maxHp[side]; s.shown.status[side] = m.status[side];
                    for (int i = 0; i < 2; ++i) s.shown.types[side][i] = m.types[side][i];
                    for (int i = 0; i < static_cast<int>(EffectSpec::Stat::COUNT); ++i) s.shown.stages[side][i] = m.stages[side][i];
                }
                s.fxSide = m.fxSide; s.fxKind = m.fxKind; s.fxAmount = m.fxAmount; s.fxText = m.fxText; s.fxTime = 0.0f;
                engine.battlePlayFx(m.attacker, m.hit);
            }
            s.messageTime += dt;
            if (s.messageTime >= MESSAGE_TIME || (skip && s.messageTime > 0.15f)) { battle->pop(); s.messageActive = false; }
        } else {
            s.messageActive = false;
            s.shown = live(*battle);
        }

        // Barras de PS: se desplazan hacia el valor mostrado.
        for (int side = 0; side < 2; ++side) {
            const float target = static_cast<float>(s.shown.hp[side]);
            if (s.bar[side] < 0.0f) s.bar[side] = target;
            const float step = BAR_SPEED * static_cast<float>(s.shown.maxHp[side]) * dt;
            s.bar[side] = s.bar[side] < target ? (std::min)(target, s.bar[side] + step) : (std::max)(target, s.bar[side] - step);
        }
        s.fxTime += dt;
        const ImVec2 foePos(40.0f, 60.0f), foeSize(360.0f, 112.0f), minePos(screen.x - 40.0f - 380.0f, screen.y - 445.0f), mineSize(380.0f, 118.0f);
        drawBox(dl, data, foePos, foeSize, s.shown, 1, s.bar[1]);
        drawBox(dl, data, minePos, mineSize, s.shown, 0, s.bar[0]);
        if (s.fxSide == 1) drawFx(dl, s, foePos, foeSize);
        else if (s.fxSide == 0) drawFx(dl, s, minePos, mineSize);

        // Cuadro de texto.
        const float boxTop = screen.y - 160.0f, boxBottom = screen.y - 30.0f;
        const float boxRight = screen.x * 0.5f;
        GuiCards::panel(dl, ImVec2(30.0f, boxTop), ImVec2(boxRight, boxBottom));
        std::string line = s.messageActive ? s.text : std::string();
        const bool choosing = battle->ready() && battle->phase() != Battle::Phase::OVER;
        if (!s.messageActive) {
            if (battle->phase() == Battle::Phase::FORCED_SWITCH) line = "Tu pokémon se ha debilitado. Elige al siguiente.";
            else if (choosing) line = "¿Qué hará " + battle->fighter(0).name + "?";
            else if (battle->phase() == Battle::Phase::OVER) line = battle->outcome() == Battle::Outcome::WON ? "¡Has ganado el combate!" : "Has perdido el combate...";
        }
        ImGui::SetCursorScreenPos(ImVec2(48.0f, boxTop + 18.0f));
        ImGui::PushTextWrapPos(boxRight - 18.0f - ImGui::GetWindowPos().x);
        ImGui::PushFont(ImGui::GetFont());
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopFont();
        ImGui::PopTextWrapPos();

        const float menuLeft = screen.x * 0.5f + 20.0f, menuRight = screen.x - 40.0f;
        bool dummyNeeded = true;
        if (choosing) {
            const bool forced = battle->phase() == Battle::Phase::FORCED_SWITCH;
            if (forced) s.menu = Menu::TEAM;
            if (s.menu == Menu::MAIN) {
                const float w = (menuRight - menuLeft - 14.0f) * 0.5f;
                ImGui::SetCursorScreenPos(ImVec2(menuLeft, boxTop + 10.0f));
                if (GuiCards::button("##fight", "LUCHAR", ImVec2(w, 52.0f), true, IM_COL32(200, 70, 50, 255))) s.menu = Menu::MOVES;
                ImGui::SetCursorScreenPos(ImVec2(menuLeft + w + 14.0f, boxTop + 10.0f));
                if (GuiCards::button("##team", "POKÉMON", ImVec2(w, 52.0f), true, IM_COL32(60, 150, 90, 255))) s.menu = Menu::TEAM;
                ImGui::SetCursorScreenPos(ImVec2(menuLeft, boxTop + 72.0f));
                GuiCards::button("##bag", "MOCHILA (próximamente)", ImVec2(w, 46.0f), false);
                ImGui::SetCursorScreenPos(ImVec2(menuLeft + w + 14.0f, boxTop + 72.0f));
                GuiCards::button("##run", "NO SE PUEDE HUIR", ImVec2(w, 46.0f), false);
            } else if (s.menu == Menu::MOVES) {
                const Battle::Fighter& me = battle->fighter(0);
                const bool struggle = battle->mustStruggle();
                const float w = (menuRight - menuLeft - 14.0f) * 0.5f, h = 56.0f;
                int hovered = -1;
                if (struggle) {
                    ImGui::SetCursorScreenPos(ImVec2(menuLeft, boxTop - 72.0f));
                    if (GuiCards::button("##struggle", "FORCEJEO (sin PP)", ImVec2(menuRight - menuLeft, 56.0f), true, IM_COL32(150, 70, 70, 255))) { Battle::Action a; a.index = 0; battle->submit(a); s.menu = Menu::MAIN; }
                }
                for (int i = 0; i < MoveRules::MAX_KNOWN && !struggle; ++i) {
                    const float x = menuLeft + (i % 2) * (w + 14.0f), y = boxTop - 150.0f + (i / 2) * (h + 10.0f) + (i / 2 ? 0.0f : 0.0f);
                    const Move* move = data.move(me.moves[i]);
                    ImGui::SetCursorScreenPos(ImVec2(x, y));
                    ImGui::PushID(i);
                    const bool usable = move && battle->canUse(i);
                    const bool clicked = ImGui::InvisibleButton("##move", ImVec2(w, h)) && usable;
                    const bool over = ImGui::IsItemHovered();
                    ImGui::PopID();
                    if (over && move) hovered = i;
                    const ImU32 fill = !move ? IM_COL32(40, 40, 50, 160) : !usable ? IM_COL32(60, 60, 70, 220) : GuiCards::typeColor(move->typeId);
                    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), GuiCards::withAlpha(fill, over && usable ? 255 : 205), 12.0f);
                    if (over && usable) dl->AddRect(ImVec2(x - 2.0f, y - 2.0f), ImVec2(x + w + 2.0f, y + h + 2.0f), GuiCards::SELECT, 14.0f, 0, 2.0f);
                    if (move) {
                        GuiCards::text(dl, ImVec2(x + 12.0f, y + 6.0f), IM_COL32(255, 255, 255, 255), move->name.c_str(), 1.0f);
                        char info[48];
                        std::snprintf(info, sizeof(info), "%s   PP %d/%d", typeName(data, move->typeId).c_str(), me.pp[i], move->pp);
                        GuiCards::text(dl, ImVec2(x + 12.0f, y + 31.0f), IM_COL32(255, 255, 255, 225), info, 0.8f);
                        const Effect effect = effectiveness(data, *move, battle->fighter(1));
                        if (*effect.label) GuiCards::chip(dl, ImVec2(x + w - 12.0f - GuiCards::textWidth(effect.label, 0.7f) - 16.0f, y + 6.0f), effect.label, effect.color, 0.7f);
                    } else GuiCards::centeredText(dl, ImVec2(x + w * 0.5f, y + h * 0.5f), GuiStyle::MUTED, "—", 1.0f);
                    if (clicked) { Battle::Action a; a.index = i; battle->submit(a); s.menu = Menu::MAIN; }
                }
                if (hovered >= 0) {
                    const Move* move = data.move(me.moves[hovered]);
                    std::string detailLine = move->description;
                    char info[96];
                    std::snprintf(info, sizeof(info), "  [%s · Potencia %s · Precisión %s]", move->kind == MoveClass::Kind::PHYSICAL ? "Físico" : move->kind == MoveClass::Kind::SPECIAL ? "Especial" : "Estado",
                                  move->power > 0 ? std::to_string(move->power).c_str() : "—", move->accuracy > 0 ? std::to_string(move->accuracy).c_str() : "—");
                    ImGui::SetCursorScreenPos(ImVec2(menuLeft, boxTop + 78.0f));
                    ImGui::PushTextWrapPos(menuRight - ImGui::GetWindowPos().x);
                    ImGui::TextUnformatted((detailLine + info).c_str());
                    ImGui::PopTextWrapPos();
                }
                ImGui::SetCursorScreenPos(ImVec2(menuLeft, boxBottom - 34.0f));
                if (GuiCards::button("##back", "< ATRÁS", ImVec2(110.0f, 30.0f), true, IM_COL32(70, 70, 92, 255))) s.menu = Menu::MAIN;
            } else { // equipo
                const float rowH = 48.0f, w = menuRight - menuLeft;
                const int n = static_cast<int>(battle->team(0).size());
                const float top = boxTop - 10.0f - static_cast<float>(n) * (rowH + 6.0f) + (forced ? 0.0f : 0.0f);
                for (int i = 0; i < n; ++i) {
                    const Battle::Fighter& f = battle->team(0)[static_cast<size_t>(i)];
                    const float y = top + static_cast<float>(i) * (rowH + 6.0f);
                    ImGui::SetCursorScreenPos(ImVec2(menuLeft, y));
                    ImGui::PushID(100 + i);
                    const bool ok = battle->canSwitchTo(i);
                    const bool clicked = ImGui::InvisibleButton("##member", ImVec2(w, rowH)) && ok;
                    const bool over = ImGui::IsItemHovered() && ok;
                    ImGui::PopID();
                    dl->AddRectFilled(ImVec2(menuLeft, y), ImVec2(menuLeft + w, y + rowH), over ? IM_COL32(70, 70, 96, 255) : i == battle->active(0) ? IM_COL32(50, 80, 60, 230) : GuiStyle::SURFACE, 12.0f);
                    ItemIcon::creature(dl, ImVec2(menuLeft + 28.0f, y + rowH * 0.5f), 17.0f, f.speciesId);
                    char text[64];
                    std::snprintf(text, sizeof(text), "%s  Nv. %d", f.name.c_str(), f.level);
                    GuiCards::text(dl, ImVec2(menuLeft + 56.0f, y + 6.0f), IM_COL32(255, 255, 255, f.fainted() ? 130 : 255), text, 0.95f);
                    float tx = menuLeft + 56.0f + GuiCards::textWidth(text, 0.95f) + 10.0f;
                    for (const int typeId : f.types) if (const PokemonType* type = data.type(typeId)) tx += GuiCards::chip(dl, ImVec2(tx, y + 6.0f), typeName(data, typeId).c_str(), GuiCards::typeColor(typeId), 0.65f) + 4.0f;
                    GuiCards::bar(dl, ImVec2(menuLeft + 56.0f, y + 31.0f), w - 160.0f, f.hpFraction(), hpColor(f.hpFraction()), 8.0f);
                    std::snprintf(text, sizeof(text), f.fainted() ? "Debilitado" : i == battle->active(0) ? "En combate" : "%d/%d", f.hp, f.maxHp);
                    if (!f.fainted() && i != battle->active(0)) std::snprintf(text, sizeof(text), "%d/%d", f.hp, f.maxHp);
                    GuiCards::text(dl, ImVec2(menuLeft + w - 90.0f, y + 15.0f), IM_COL32(255, 255, 255, 220), text, 0.8f);
                    if (clicked) {
                        if (forced) battle->forceSwitch(i);
                        else { Battle::Action a; a.kind = Battle::Action::Kind::SWITCH; a.index = i; battle->submit(a); }
                        s.menu = Menu::MAIN;
                    }
                }
                if (!forced) {
                    ImGui::SetCursorScreenPos(ImVec2(menuLeft, boxBottom - 34.0f));
                    if (GuiCards::button("##back", "< ATRÁS", ImVec2(110.0f, 30.0f), true, IM_COL32(70, 70, 92, 255))) s.menu = Menu::MAIN;
                }
            }
        } else if (battle->phase() == Battle::Phase::OVER && !battle->hasMessage()) {
            ImGui::SetCursorScreenPos(ImVec2(menuLeft, boxTop + 30.0f));
            if (GuiCards::button("##done", "CONTINUAR", ImVec2(menuRight - menuLeft, 56.0f), true, GuiStyle::ACCENT)) {
                engine.battleFinish();
                state = GameState::PLAYING;
                return;
            }
        } else if (battle->hasMessage()) {
            GuiCards::text(dl, ImVec2(boxRight - 150.0f, boxBottom - 24.0f), GuiStyle::MUTED, "Clic: continuar", 0.8f);
        }
        if (dummyNeeded) ImGui::Dummy(ImVec2(0.0f, 0.0f)); // los SetCursorScreenPos de arriba necesitan un elemento al final
    }
}
