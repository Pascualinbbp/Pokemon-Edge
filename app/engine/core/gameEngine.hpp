#pragma once
#include "../world/scene.hpp"
#include "../render/renderer3D.hpp"
#include "input.hpp"

class GameEngine {
    public:
    // Fondo propio del juego (independiente del fondo de la GUI).
    static constexpr float CLEAR_COLOR[4] = { 0.09f, 0.10f, 0.13f, 1.0f };

    void init(ID3D11Device* device) {
        m_renderer.init(device);
    }

    void update(float dt, const InputState& input) {
        m_scene.update(dt, input);
    }

    void render(ID3D11DeviceContext* context, int screenW, int screenH) {
        m_renderer.render(context, m_scene, screenW, screenH);
    }

    void cleanup() {
        m_renderer.cleanup();
    }

    private:
    Scene m_scene;
    Renderer3D m_renderer;
};