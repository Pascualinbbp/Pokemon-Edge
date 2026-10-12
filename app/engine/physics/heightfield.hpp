#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace Physics {
    // Valores en una malla regular (CELL metros) generada una sola vez al crear el mundo: la altura del terreno, la de la
    // superficie del agua, etc. Consultarla es una interpolación bilineal, así que cualquier entidad puede preguntarla
    // cada paso sin coste apreciable.
    class Heightfield {
        public:
        static constexpr float CELL = 2.0f;

        // Malla vacía (todo 'fill') que cubre el cuadrado [-half, half].
        void init(float half, float fill = 0.0f) {
            m_half = half;
            m_count = static_cast<int>(std::lround(2.0f * half / CELL)) + 1;
            m_values.assign(static_cast<size_t>(m_count) * m_count, fill);
        }

        // Rellena la malla con f(x, z).
        template <typename F>
        void build(float half, F f) {
            init(half);
            for (int row = 0; row < m_count; ++row) {
                for (int col = 0; col < m_count; ++col) at(col, row) = f(nodeCoord(col), nodeCoord(row));
            }
        }

        bool empty() const { return m_values.empty(); }
        int count() const { return m_count; }
        float half() const { return m_half; }
        float nodeCoord(int index) const { return -m_half + static_cast<float>(index) * CELL; }
        float* data() { return m_values.data(); }
        float& at(int col, int row) { return m_values[static_cast<size_t>(row) * m_count + col]; }
        float at(int col, int row) const { return m_values[static_cast<size_t>(row) * m_count + col]; }

        // Valor en (x, z): interpolación sobre los mismos triángulos que dibuja la malla del suelo (diagonal de (col+1, row) a
        // (col, row+1)), así lo que pisa una entidad coincide con lo que se ve.
        float sample(float x, float z) const {
            if (m_values.empty()) return 0.0f;
            const float fx = (std::clamp)((x + m_half) / CELL, 0.0f, static_cast<float>(m_count - 1));
            const float fz = (std::clamp)((z + m_half) / CELL, 0.0f, static_cast<float>(m_count - 1));
            const int c0 = (std::min)(static_cast<int>(fx), m_count - 2), r0 = (std::min)(static_cast<int>(fz), m_count - 2);
            const float tx = fx - c0, tz = fz - r0;
            const float* row0 = &m_values[static_cast<size_t>(r0) * m_count + c0];
            const float* row1 = row0 + m_count;
            if (tx + tz <= 1.0f) return row0[0] + tx * (row0[1] - row0[0]) + tz * (row1[0] - row0[0]);
            return row1[1] + (1.0f - tx) * (row1[0] - row1[1]) + (1.0f - tz) * (row0[1] - row1[1]);
        }

        // Pendiente (derivada en x y en z) en un punto, por diferencias centradas.
        void gradient(float x, float z, float& gx, float& gz) const {
            constexpr float STEP = CELL * 0.5f;
            gx = (sample(x + STEP, z) - sample(x - STEP, z)) / (2.0f * STEP);
            gz = (sample(x, z + STEP) - sample(x, z - STEP)) / (2.0f * STEP);
        }

        float slope(float x, float z) const {
            float gx, gz;
            gradient(x, z, gx, gz);
            return std::sqrt(gx * gx + gz * gz);
        }

        private:
        float m_half = 0.0f;
        int m_count = 0;
        std::vector<float> m_values;
    };
}
