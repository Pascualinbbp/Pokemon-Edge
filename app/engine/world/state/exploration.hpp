#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "../../physics/physicsWorld.hpp"

// Lo que el jugador ha explorado del mundo: una cuadrícula de celdas de 2 m que se destapan alrededor de donde pasa.
// Es lo que desbloquea el mapa grande; el minimapa y el mapa comparten esta misma cuadrícula (CELL y SIZE).
class Exploration {
    public:
    static constexpr float CELL = 2.0f;
    static constexpr int SIZE = static_cast<int>(2.0f * Physics::World::HALF_SIZE / CELL);
    static constexpr float REVEAL_RADIUS = 18.0f;

    static int cellOf(float coordinate) { return static_cast<int>(std::floor((coordinate + Physics::World::HALF_SIZE) / CELL)); }
    static float centerOf(int cell) { return -Physics::World::HALF_SIZE + (cell + 0.5f) * CELL; }

    bool seen(int col, int row) const {
        return col >= 0 && row >= 0 && col < SIZE && row < SIZE && !m_cells.empty() && m_cells[static_cast<size_t>(row) * SIZE + col];
    }

    // Destapa lo que rodea al jugador (solo hace algo al cambiar de celda).
    void reveal(float x, float z) {
        if (m_cells.empty()) m_cells.assign(static_cast<size_t>(SIZE) * SIZE, 0);
        const int col = cellOf(x), row = cellOf(z);
        if (col == m_lastCol && row == m_lastRow) return;
        m_lastCol = col;
        m_lastRow = row;

        const int reach = static_cast<int>(REVEAL_RADIUS / CELL);
        for (int r = (std::max)(0, row - reach); r <= (std::min)(SIZE - 1, row + reach); ++r) {
            for (int c = (std::max)(0, col - reach); c <= (std::min)(SIZE - 1, col + reach); ++c) {
                const float dx = static_cast<float>(c - col), dz = static_cast<float>(r - row);
                if (dx * dx + dz * dz <= static_cast<float>(reach * reach)) m_cells[static_cast<size_t>(r) * SIZE + c] = 1;
            }
        }
    }

    // Guardado compacto: 4 celdas por carácter hexadecimal.
    std::string store() const {
        static const char* DIGITS = "0123456789abcdef";
        std::string text;
        if (m_cells.empty()) return text;
        text.reserve(m_cells.size() / 4 + 1);
        for (size_t i = 0; i < m_cells.size(); i += 4) {
            int value = 0;
            for (size_t k = 0; k < 4 && i + k < m_cells.size(); ++k) value |= m_cells[i + k] << k;
            text += DIGITS[value];
        }
        return text;
    }

    void restore(const std::string& text) {
        if (text.empty()) return;
        m_cells.assign(static_cast<size_t>(SIZE) * SIZE, 0);
        for (size_t i = 0; i < text.size() && i * 4 < m_cells.size(); ++i) {
            const char c = text[i];
            const int value = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0;
            for (size_t k = 0; k < 4 && i * 4 + k < m_cells.size(); ++k) m_cells[i * 4 + k] = static_cast<unsigned char>((value >> k) & 1);
        }
        m_lastCol = m_lastRow = -1000;
    }

    private:
    std::vector<unsigned char> m_cells;
    int m_lastCol = -1000, m_lastRow = -1000;
};
