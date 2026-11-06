#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

// Emisoras de radio del juego (tabla radio; las carga DatabaseManager). Cada una es un fichero de assets/audio/radio/<id>.mp3 (o .wav; se descodifica sobre la marcha, así que
// puede durar horas) y su logo assets/sprites/radio/<id>.png. Para cambiar la música de una emisora basta con sustituir su fichero; para añadir otra, una fila en la tabla y sus
// dos ficheros. La emisora 0 es "apagada".
namespace Radio {
    struct Station { int id = 0; std::string name; std::string genre; };

    inline constexpr size_t MAX = 16; // emisoras que caben en la rueda (y en el guardado de por dónde iba cada una)

    inline std::vector<Station>& stations() { static std::vector<Station> list; return list; }
    inline int count() { return static_cast<int>((std::min)(stations().size(), MAX)); }

    inline const Station& station(int number) { // 1..count()
        static const Station none;
        const int n = count();
        return n == 0 ? none : stations()[(number < 1 ? 1 : number > n ? n : number) - 1];
    }
    inline std::string music(int number) { return "audio/radio/" + std::to_string(station(number).id); }    // sin extensión
    inline std::string logo(int number) { return "sprites/radio/" + std::to_string(station(number).id) + ".png"; }

    // Emisora siguiente o anterior (0 = apagada; ciclo: apagada → 1 → ... → count() → apagada).
    inline int cycle(int current, int step) {
        const int n = count() + 1;
        return ((current + step) % n + n) % n;
    }

    // Rueda de selección (como la de la radio de GTA): la emisora apagada abajo y las demás en círculo. Todo en píxeles respecto al centro de
    // la pantalla; la comparten la lógica (qué emisora señala el cursor) y el dibujo.
    namespace Wheel {
        inline size_t slots() { return static_cast<size_t>(count()) + 1; } // 0 = apagada, 1..count() = emisoras
        inline constexpr float RING = 175.0f;           // radio del círculo de logos
        inline constexpr float ICON = 36.0f;            // radio de cada logo
        inline constexpr float OUTER = RING + ICON * 1.6f;
        inline constexpr float DEAD = 70.0f;            // zona central sin selección
        inline constexpr float REACH = OUTER * 1.25f;   // hasta dónde llega el cursor

        inline void slotCenter(int slot, float& x, float& y) {
            const float angle = 1.5707963f + static_cast<float>(slot) * 6.2831853f / static_cast<float>(slots());
            x = std::cos(angle) * RING;
            y = std::sin(angle) * RING;
        }

        // Qué señala el cursor (x, y): una emisora (0..count()), -1 en el centro o -2 fuera de la rueda.
        inline int hit(float x, float y) {
            const float d2 = x * x + y * y;
            if (d2 < DEAD * DEAD) return -1;
            if (d2 > OUTER * OUTER) return -2;
            const float turn = (std::atan2(y, x) - 1.5707963f) / (6.2831853f / static_cast<float>(slots()));
            const int n = static_cast<int>(slots());
            return ((static_cast<int>(std::lround(turn)) % n) + n) % n;
        }
    }
}
