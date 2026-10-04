#pragma once
#include "../world/scene.hpp"
#include "../render/renderer3D.hpp"
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

    void newGame() {
        m_scene = Scene{};
    }

    void applySave(const SaveData& save) {
        m_scene = Scene{};
        m_scene.player.body.position = { save.playerPosition[0], save.playerPosition[1], save.playerPosition[2] };
    }

    SaveData captureSave() const {
        const DirectX::XMFLOAT3& p = m_scene.player.body.position;
        SaveData save;
        save.playerPosition = { p.x, p.y, p.z };
        return save;
    }

    void update(float dt, const InputState& input) {
        m_scene.update(dt, input);
    }

    void render(ID3D11DeviceContext* context, int screenW, int screenH) {
        m_renderer.render(context, m_scene, screenW, screenH);
    }

    GameStatus status() const {
        return { m_scene.camera.aimBlend(), m_scene.captures, m_scene.captureNotice > 0.0f };
    }

    void cleanup() {
        m_renderer.cleanup();
    }

    private:
    Scene m_scene;
    Renderer3D m_renderer;
};
