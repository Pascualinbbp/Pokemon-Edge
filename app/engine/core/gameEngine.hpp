#pragma once
#include <utility>
#include <vector>
#include "../world/scene.hpp"
#include "../render/renderer3D.hpp"
#include "../../models/pokeballType.hpp"
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

    // Tipos de pokéball de la base de datos: con ellos se crea el inventario de cada partida.
    void setBallTypes(std::vector<PokeballType> types) {
        m_ballTypes = std::move(types);
    }

    void newGame() {
        m_scene = Scene(m_ballTypes);
    }

    void applySave(const SaveData& save) {
        m_scene = Scene(m_ballTypes);
        m_scene.player.body.position = { save.playerPosition[0], save.playerPosition[1], save.playerPosition[2] };
        m_scene.dayCycle.setTime(save.worldTime);
        m_scene.inventory.restore(save.balls, save.selectedBall);
    }

    SaveData captureSave() const {
        const DirectX::XMFLOAT3& p = m_scene.player.body.position;
        SaveData save;
        save.playerPosition = { p.x, p.y, p.z };
        save.worldTime = m_scene.dayCycle.time();
        m_scene.inventory.store(save.balls, save.selectedBall);
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

    GameStatus status() const {
        return m_scene.status();
    }

    void cleanup() {
        m_renderer.cleanup();
    }

    private:
    std::vector<PokeballType> m_ballTypes;
    Scene m_scene;
    Renderer3D m_renderer;
};
