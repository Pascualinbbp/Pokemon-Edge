#pragma once
#include <utility>
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
        m_scene.player.body.position = { save.playerPosition[0], save.playerPosition[1], save.playerPosition[2] };
        m_scene.dayCycle.setTime(save.worldTime);
        m_scene.exploration.restore(save.explored);
        m_scene.inventory.restore(save.items, save.selectedBall, save.money);
        m_scene.storage.restore(save.team, save.pc, save.autoReleaseRank, save.activeIndex);
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
        save.autoReleaseRank = m_scene.storage.autoRankIndex();
        save.activeIndex = m_scene.storage.activeIndex();
        save.playerLevel = m_scene.progress.level();
        save.playerXp = m_scene.progress.xp();
        save.badges = m_scene.progress.badges();
        return save;
    }

    // Devuelve true si se pide pausar (ESC fuera del modo lanzamiento).
    bool update(float dt, const InputState& input) {
        m_scene.dayCycle.update(dt); // el tiempo del mundo corre siempre mientras se juega, haga lo que haga el jugador
        return m_scene.update(dt, input);
    }

    void render(ID3D11DeviceContext* context, int screenW, int screenH) {
        m_renderer.render(context, m_scene, screenW, screenH);
    }

    // Lo que necesita la pantalla de inventario (el equipo se modifica desde ella).
    const GameData& data() const { return m_data; }
    Inventory& inventory() { return m_scene.inventory; }
    PokemonStorage& storage() { return m_scene.storage; }
    const PlayerProgress& progress() const { return m_scene.progress; }

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
    GameData m_data;
    Scene m_scene;
    Renderer3D m_renderer;
};
