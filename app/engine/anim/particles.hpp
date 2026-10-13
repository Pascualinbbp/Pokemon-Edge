#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <DirectXMath.h>
#include "easing.hpp"
#include "../../utils/core/randomUtil.hpp"

namespace Anim {
    enum class Shape { CUBE, STAR };

    // Descripción de una ráfaga de partículas: todo son parámetros, no hay nada específico de un efecto.
    struct Burst {
        int count = 8;
        float lifeMin = 0.6f, lifeMax = 1.0f;     // segundos
        float speedMin = 1.0f, speedMax = 2.0f;   // m/s
        float spread = 3.14159265f;               // semiángulo del cono alrededor del eje (PI = esfera entera)
        float gravity = 0.0f;                     // m/s² hacia abajo
        float drag = 0.0f;                        // frenado por segundo
        float sizeStart = 0.1f, sizeEnd = 0.0f;
        DirectX::XMFLOAT3 colorStart = { 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 colorEnd = { 1.0f, 1.0f, 1.0f };
        float spin = 6.0f;                        // rad/s (el signo y la magnitud varían por partícula)
        Shape shape = Shape::CUBE;
    };

    struct Particle {
        DirectX::XMFLOAT3 position = {}, velocity = {};
        float age = 0.0f, life = 1.0f;
        float sizeStart = 0.1f, sizeEnd = 0.0f;
        float angle = 0.0f, spin = 0.0f;
        float gravity = 0.0f, drag = 0.0f;
        DirectX::XMFLOAT3 colorStart = {}, colorEnd = {};
        Shape shape = Shape::CUBE;

        float u() const { return Easing::clamp01(age / life); }
        float size() const { return Easing::lerp(sizeStart, sizeEnd, u()); }
        DirectX::XMFLOAT3 color() const {
            const float t = u();
            return { Easing::lerp(colorStart.x, colorEnd.x, t), Easing::lerp(colorStart.y, colorEnd.y, t), Easing::lerp(colorStart.z, colorEnd.z, t) };
        }
    };

    // Sistema de partículas sencillo en CPU: ráfagas que se simulan y mueren solas. Quien lo dibuja recorre particles().
    class ParticleSystem {
        public:
        void burst(const Burst& config, const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& axis = { 0.0f, 1.0f, 0.0f }) {
            using DirectX::XMFLOAT3;
            const float length = (std::max)(std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z), 1.0e-4f);
            const XMFLOAT3 up = { axis.x / length, axis.y / length, axis.z / length };
            // Base perpendicular al eje.
            const XMFLOAT3 helper = std::fabs(up.y) < 0.9f ? XMFLOAT3{ 0.0f, 1.0f, 0.0f } : XMFLOAT3{ 1.0f, 0.0f, 0.0f };
            XMFLOAT3 side = { up.y * helper.z - up.z * helper.y, up.z * helper.x - up.x * helper.z, up.x * helper.y - up.y * helper.x };
            const float sideLength = std::sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
            side = { side.x / sideLength, side.y / sideLength, side.z / sideLength };
            const XMFLOAT3 front = { up.y * side.z - up.z * side.y, up.z * side.x - up.x * side.z, up.x * side.y - up.y * side.x };

            for (int i = 0; i < config.count; ++i) {
                // Dirección uniforme dentro del cono: coseno del ángulo polar entre cos(spread) y 1.
                const float cosTheta = RandomUtil::range(std::cos(config.spread), 1.0f);
                const float sinTheta = std::sqrt((std::max)(0.0f, 1.0f - cosTheta * cosTheta));
                const float phi = RandomUtil::range(0.0f, 6.2831853f);
                const float a = std::cos(phi) * sinTheta, b = std::sin(phi) * sinTheta;
                const XMFLOAT3 dir = { up.x * cosTheta + side.x * a + front.x * b, up.y * cosTheta + side.y * a + front.y * b, up.z * cosTheta + side.z * a + front.z * b };
                const float speed = RandomUtil::range(config.speedMin, config.speedMax);

                Particle p;
                p.position = origin;
                p.velocity = { dir.x * speed, dir.y * speed, dir.z * speed };
                p.life = RandomUtil::range(config.lifeMin, config.lifeMax);
                p.sizeStart = config.sizeStart;
                p.sizeEnd = config.sizeEnd;
                p.angle = RandomUtil::range(0.0f, 6.2831853f);
                p.spin = config.spin * RandomUtil::range(-1.0f, 1.0f);
                p.gravity = config.gravity;
                p.drag = config.drag;
                p.colorStart = config.colorStart;
                p.colorEnd = config.colorEnd;
                p.shape = config.shape;
                m_particles.push_back(p);
            }
        }

        void update(float dt) {
            for (Particle& p : m_particles) {
                p.age += dt;
                const float keep = (std::max)(0.0f, 1.0f - p.drag * dt);
                p.velocity.x *= keep;
                p.velocity.z *= keep;
                p.velocity.y = p.velocity.y * keep - p.gravity * dt;
                p.position.x += p.velocity.x * dt;
                p.position.y += p.velocity.y * dt;
                p.position.z += p.velocity.z * dt;
                p.angle += p.spin * dt;
            }
            m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(), [](const Particle& p) { return p.age >= p.life; }), m_particles.end());
        }

        void clear() { m_particles.clear(); }
        bool empty() const { return m_particles.empty(); }
        const std::vector<Particle>& particles() const { return m_particles; }

        private:
        std::vector<Particle> m_particles;
    };
}
