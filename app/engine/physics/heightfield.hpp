#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace Physics {
    // Altura del terreno en una malla regular (1 m) muestreada una sola vez al generar el mundo; consultarla es una
    // interpolación bilineal, así que cualquier entidad puede preguntarla cada paso sin coste apreciable.
    class Heightfield {
        public:
        static constexpr float CELL = 1.0f;

        // Rellena la malla con f(x, z) en el cuadrado [-half, half].
        template <typename F>
        void build(float half, F f) {
            m_half = half;
            m_count = static_cast<int>(std::lround(2.0f * half / CELL)) + 1;
            m_heights.resize(static_cast<size_t>(m_count) * m_count);
            for (int row = 0; row < m_count; ++row) {
                for (int col = 0; col < m_count; ++col) m_heights[static_cast<size_t>(row) * m_count + col] = f(-half + col * CELL, -half + row * CELL);
            }
        }

        bool empty() const { return m_heights.empty(); }

        float sample(float x, float z) const {
            if (m_heights.empty()) return 0.0f;
            const float fx = (std::clamp)((x + m_half) / CELL, 0.0f, static_cast<float>(m_count - 1));
            const float fz = (std::clamp)((z + m_half) / CELL, 0.0f, static_cast<float>(m_count - 1));
            const int c0 = (std::min)(static_cast<int>(fx), m_count - 2), r0 = (std::min)(static_cast<int>(fz), m_count - 2);
            const float tx = fx - c0, tz = fz - r0;
            const float* row0 = &m_heights[static_cast<size_t>(r0) * m_count + c0];
            const float* row1 = row0 + m_count;
            return (row0[0] * (1.0f - tx) + row0[1] * tx) * (1.0f - tz) + (row1[0] * (1.0f - tx) + row1[1] * tx) * tz;
        }

        private:
        float m_half = 0.0f;
        int m_count = 0;
        std::vector<float> m_heights;
    };
}
