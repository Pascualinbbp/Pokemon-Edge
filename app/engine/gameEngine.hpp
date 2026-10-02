#pragma once
#include "scene.hpp"
#include "renderer3D.hpp"
#include "input.hpp"

class GameEngine {
public:
    void init(ID3D11Device* device) {
        m_renderer.init(device);
    }
    
    void update(float dt, const InputState& input) {
        m_scene.player.update(dt, input);
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