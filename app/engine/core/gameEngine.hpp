#pragma once
#include "../../utils/core/crashUtil.hpp"
#include <cstdio>
#include <utility>
#include "../../utils/core/loggerUtil.hpp"
#include <vector>
#include "../world/scene.hpp"
#include "../render/renderer3D.hpp"
#include "../../models/gameData.hpp"
#include "../../models/saveData.hpp"
#include "gameStatus.hpp"
#include "input.hpp"

class GameEngine {
    public:
    // Fondo propio del juego (independiente del fondo de la GUI).
    static constexpr float CLEAR_COLOR[4] = { 0.09f, 0.10f, 0.13f, 1.0f };

    void init(ID3D11Device* device) {
        m_renderer.init(device);
    }

    // Datos de juego de la base de datos (pokéballs, objetos, cofres...): con ellos se crea cada partida.
    // El mundo no se genera aquí sino al empezar o cargar una partida (newGame / applySave).
    void setData(GameData data) {
        m_data = std::move(data);
    }

    void newGame() {
        m_scene = Scene(m_data);
    }

    void applySave(const SaveData& save) {
        m_scene = save.worldSeed != 0 ? Scene(m_data, save.worldSeed) : Scene(m_data);
        float x = save.playerPosition[0], z = save.playerPosition[2];
        if (!m_scene.habitats.playable(x, z)) x = z = 0.0f; // partidas de antes del mundo nuevo: el inicio
        m_scene.player.body.position = { x, (std::max)(save.playerPosition[1], m_scene.world.surfaceHeight(x, z)) + 0.5f, z };
        m_scene.dayCycle.setTime(save.worldTime);
        m_scene.exploration.restore(save.explored);
        m_scene.inventory.restore(save.items, save.selectedBall, save.money);
        m_scene.storage.restore(save.team, save.pc, save.autoReleaseRank, save.activeIndex);
        m_scene.storage.restoreDex(save.dexSeen, save.dexCaught);
        m_scene.storage.dexProgress.restore(save.dexProgress);
        m_scene.progress.restore(save.playerLevel, save.playerXp, save.badges);
    }

    SaveData captureSave() const {
        const DirectX::XMFLOAT3& p = m_scene.player.body.position;
        SaveData save;
        save.playerPosition = { p.x, p.y, p.z };
        save.worldTime = m_scene.dayCycle.time();
        save.worldSeed = m_scene.habitats.seed();
        save.explored = m_scene.exploration.store();
        m_scene.inventory.store(save.items, save.selectedBall, save.money);
        m_scene.storage.store(save.team, save.pc);
        m_scene.storage.storeDex(save.dexSeen, save.dexCaught);
        m_scene.storage.dexProgress.store(save.dexProgress);
        save.autoReleaseRank = m_scene.storage.autoRankIndex();
        save.activeIndex = m_scene.storage.activeIndex();
        save.playerLevel = m_scene.progress.level();
        save.playerXp = m_scene.progress.xp();
        save.badges = m_scene.progress.badges();
        save.summary = { {}, save.playerLevel, save.badges, m_scene.inventory.money(), m_scene.storage.dexCount(PokemonStorage::DEX_CAUGHT) };
        for (const OwnedPokemon& owned : m_scene.storage.team()) save.summary.team.push_back({ owned.speciesId, owned.level, owned.shiny });
        return save;
    }

    // Devuelve true si se pide pausar (ESC fuera del modo lanzamiento).
    bool update(float dt, const InputState& input) {
        const DirectX::XMFLOAT3& at = m_scene.player.body.position;
        CrashUtil::detail("x=%.1f y=%.1f z=%.1f", at.x, at.y, at.z);
        CrashUtil::stage("juego: actualizar");
        m_scene.dayCycle.update(dt); // el tiempo del mundo corre siempre mientras se juega, haga lo que haga el jugador
        const bool pause = m_scene.update(dt, input);
        logLayer();
        return pause;
    }

    void render(ID3D11DeviceContext* context, int screenW, int screenH) {
        CrashUtil::stage("juego: dibujar");
        m_renderer.render(context, m_scene, screenW, screenH);
        CrashUtil::stage("juego: dibujado");
    }

    // Lo que necesita la pantalla de inventario (el equipo se modifica desde ella).
    const GameData& data() const { return m_data; }
    Inventory& inventory() { return m_scene.inventory; }
    PokemonStorage& storage() { return m_scene.storage; }
    const PlayerProgress& progress() const { return m_scene.progress; }
    PlayerProgress& progress() { return m_scene.progress; }

    bool needsStarter() const { return m_scene.needsStarter(); }
    void chooseStarter(int speciesId) { m_scene.chooseStarter(speciesId); }

    // true una sola vez cuando el jugador acaba de usar la máquina de investigación.
    bool takeResearchRequest() {
        const bool requested = m_scene.researchRequested;
        m_scene.researchRequested = false;
        return requested;
    }

    GameStatus status() const {
        GameStatus s = m_scene.status();
        DirectX::XMStoreFloat4x4(&s.viewProj, m_scene.getViewMatrix() * DirectX::XMLoadFloat4x4(&m_renderer.projection()));
        return s;
    }

    void cleanup() {
        m_renderer.cleanup();
    }

    private:
    // Deja en el registro cada vez que el jugador entra o sale de una cueva, una mina o el cielo (para localizar fallos en cada capa).
    void logLayer() {
        const Physics::Body& body = m_scene.player.body;
        const int layer = static_cast<int>(m_scene.habitats.layerAt(body.position.x, body.position.z, body.feet()));
        if (layer == m_layer) return;
        m_layer = layer;
        static constexpr const char* NAMES[] = { "subsuelo", "superficie", "cielo" };
        char text[160];
        std::snprintf(text, sizeof(text), "Capa %s en (%.1f, %.1f, %.1f)", NAMES[(layer >= 0 && layer < 3) ? layer : 1], body.position.x, body.position.y, body.position.z);
        Logger::logInfo("ENGINE", text);
    }

    int m_layer = -1;
    GameData m_data;
    Scene m_scene;
    Renderer3D m_renderer;
};
