#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <DirectXMath.h>
#include "../core/input.hpp"
#include "../physics/physicsWorld.hpp"
#include "camera.hpp"
#include "captureTarget.hpp"
#include "player.hpp"
#include "pokeball.hpp"

struct Scene {
    static constexpr float HALF_SIZE = Physics::World::HALF_SIZE;

    // Lanzamiento
    static constexpr float THROW_SPEED = 26.0f;       // m/s (alcance máximo ~34 m con g = 20)
    static constexpr float THROW_COOLDOWN = 0.45f;    // segundos entre lanzamientos
    static constexpr float MAX_AIM_DISTANCE = 25.0f;  // si el rayo de apuntado no choca con nada, se apunta a esta distancia
    static constexpr float SPAWN_SIDE = 0.35f;        // la pokéball sale por el hombro derecho
    static constexpr float SPAWN_FORWARD = 0.5f;
    static constexpr float SPAWN_HEIGHT = 1.1f;
    static constexpr size_t MAX_BALLS = 12;
    static constexpr float MAX_SUBSTEP = 1.0f / 60.0f; // paso máximo de la física de los proyectiles
    static constexpr float NOTICE_TIME = 2.0f;

    Physics::World world;
    Player player;
    Camera camera;
    CaptureTarget target;
    std::vector<Pokeball> balls;

    int captures = 0;
    float captureNotice = 0.0f; // segundos restantes del aviso "¡Capturado!"
    float throwCooldown = 0.0f;

    void update(float dt, const InputState& input) {
        camera.rotate(input.lookX, input.lookY);
        camera.update(dt, input.aim);

        player.aiming = input.aim;
        player.update(dt, input, camera.yaw(), world);

        throwCooldown = (std::max)(0.0f, throwCooldown - dt);
        captureNotice = (std::max)(0.0f, captureNotice - dt);
        if (input.aim && input.throwBall && throwCooldown <= 0.0f) throwBall();

        target.update(dt);
        updateBalls(dt);
    }

    // Recorre todos los cuerpos de la escena (jugador, objetivo y pokéballs; después NPCs, pokémon...).
    // Es el único sitio que hay que ampliar al añadir entidades: sombras y física lo usan.
    template <typename Fn>
    void forEachBody(Fn&& fn) const {
        fn(player.body);
        fn(target.body);
        for (const Pokeball& ball : balls) fn(ball.body);
    }

    DirectX::XMMATRIX getViewMatrix() const {
        return camera.viewMatrix(player.body.position);
    }

    private:
    // Lanza una pokéball hacia el punto que señala la cruceta (centro de la pantalla).
    void throwBall() {
        using namespace DirectX;
        const XMFLOAT3 eye = camera.eye(player.body.position);
        const XMFLOAT3 dir = camera.forward();

        // Punto apuntado: primer impacto del rayo de la cámara con el suelo o el objetivo.
        float distance = MAX_AIM_DISTANCE;
        const float ground = world.groundHeight(eye.x, eye.z);
        if (dir.y < -1.0e-4f) distance = (std::min)(distance, (std::max)((ground - eye.y) / dir.y, 0.0f));
        float hit = 0.0f;
        if (target.raycast(eye, dir, hit)) distance = (std::min)(distance, hit);
        const XMFLOAT3 aim = { eye.x + dir.x * distance, eye.y + dir.y * distance, eye.z + dir.z * distance };

        float sinYaw, cosYaw;
        XMScalarSinCos(&sinYaw, &cosYaw, camera.yaw());
        const XMFLOAT3& p = player.body.position;

        Pokeball ball;
        ball.body.position = { p.x + cosYaw * SPAWN_SIDE + sinYaw * SPAWN_FORWARD,
                               p.y + SPAWN_HEIGHT * player.heightScale(),
                               p.z - sinYaw * SPAWN_SIDE + cosYaw * SPAWN_FORWARD };
        ball.body.velocity = launchVelocity(ball.body.position, aim, dir);

        if (balls.size() >= MAX_BALLS) balls.erase(balls.begin());
        balls.push_back(ball);
        throwCooldown = THROW_COOLDOWN;
    }

    // Velocidad inicial (módulo THROW_SPEED) para que la parábola pase por 'aim' (trayectoria baja).
    // Si el punto está fuera de alcance, se lanza a 45° (máximo alcance). Si está pegado al jugador,
    // se lanza en línea recta según la cámara.
    static DirectX::XMFLOAT3 launchVelocity(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& aim,
                                            const DirectX::XMFLOAT3& cameraDir) {
        const float dx = aim.x - from.x;
        const float dz = aim.z - from.z;
        const float dy = aim.y - from.y;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (d < 0.5f) return { cameraDir.x * THROW_SPEED, cameraDir.y * THROW_SPEED, cameraDir.z * THROW_SPEED };

        const float v2 = THROW_SPEED * THROW_SPEED;
        const float g = Physics::World::GRAVITY;
        const float discriminant = v2 * v2 - g * (g * d * d + 2.0f * dy * v2);
        const float tanA = discriminant >= 0.0f ? (v2 - std::sqrt(discriminant)) / (g * d) : 1.0f;

        const float cosA = 1.0f / std::sqrt(1.0f + tanA * tanA);
        const float sinA = tanA * cosA;
        return { dx / d * THROW_SPEED * cosA, THROW_SPEED * sinA, dz / d * THROW_SPEED * cosA };
    }

    // Física de las pokéballs en pasos pequeños (así una bola rápida no atraviesa el objetivo).
    void updateBalls(float dt) {
        if (balls.empty()) return;

        const int steps = (std::max)(1, static_cast<int>(std::ceil(dt / MAX_SUBSTEP)));
        const float h = dt / static_cast<float>(steps);
        for (int i = 0; i < steps; ++i) {
            for (Pokeball& ball : balls) {
                if (ball.expired()) continue;

                world.step(ball.body, h);
                ball.age += h;
                if (target.hitBy(ball.body.position, Pokeball::RADIUS)) {
                    target.capture();
                    ++captures;
                    captureNotice = NOTICE_TIME;
                    ball.age = Pokeball::LIFETIME; // la pokéball se consume
                }
            }
        }
        balls.erase(std::remove_if(balls.begin(), balls.end(), [](const Pokeball& b) { return b.expired(); }), balls.end());
    }
};
