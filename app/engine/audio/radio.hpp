#pragma once
#include <cmath>
#include <cstddef>

// Emisoras de radio del juego. Cada una es un fichero de assets/audio/radio/ (MP3 —o WAV—, sin extensión aquí; se descodifica sobre la marcha, así que puede durar horas). Para cambiar la música de una
// emisora basta con sustituir su fichero; para añadir otra, una fila aquí. La emisora 0 es "apagada".
namespace Radio {
    struct Station { const char* name; const char* genre; const char* path; const char* logo; }; // logo: assets/hud/<logo>.png

    inline constexpr size_t COUNT = 10;

    inline const Station& station(int number) { // 1..COUNT
        static const Station table[COUNT] = {
            { "Ruta 1 FM",           "Pop",         "audio/radio/01_pop", "radio/01" },
            { "Lofi Centro Pokémon", "Lofi",        "audio/radio/02_lofi", "radio/02" },
            { "Kanto Rock",          "Rock",        "audio/radio/03_rock", "radio/03" },
            { "Chiptune 8-bit",      "Chiptune",    "audio/radio/04_chip", "radio/04" },
            { "Jazz Nocturno",       "Jazz",        "audio/radio/05_jazz", "radio/05" },
            { "Neón Electro",        "Electrónica", "audio/radio/06_electro", "radio/06" },
            { "Reggae de la Playa",  "Reggae",      "audio/radio/07_reggae", "radio/07" },
            { "Orquesta Épica",      "Orquestal",   "audio/radio/08_epic", "radio/08" },
            { "Funk del Gimnasio",   "Funk",        "audio/radio/09_funk", "radio/09" },
            { "Ambient del Cielo",   "Ambiental",   "audio/radio/10_ambient", "radio/10" },
        };
        return table[(number < 1 ? 1 : number > static_cast<int>(COUNT) ? static_cast<int>(COUNT) : number) - 1];
    }

    // Emisora siguiente o anterior (0 = apagada; ciclo: apagada → 1 → ... → COUNT → apagada).
    inline int cycle(int current, int step) {
        const int n = static_cast<int>(COUNT) + 1;
        return ((current + step) % n + n) % n;
    }

    // Rueda de selección (como la de la radio de GTA): la emisora apagada abajo y las demás en círculo. Todo en píxeles respecto al centro de
    // la pantalla; la comparten la lógica (qué emisora señala el cursor) y el dibujo.
    namespace Wheel {
        inline constexpr size_t SLOTS = COUNT + 1;      // 0 = apagada, 1..COUNT = emisoras
        inline constexpr float RING = 175.0f;           // radio del círculo de logos
        inline constexpr float ICON = 36.0f;            // radio de cada logo
        inline constexpr float OUTER = RING + ICON * 1.6f;
        inline constexpr float DEAD = 70.0f;            // zona central sin selección
        inline constexpr float REACH = OUTER * 1.25f;   // hasta dónde llega el cursor

        inline void slotCenter(int slot, float& x, float& y) {
            const float angle = 1.5707963f + static_cast<float>(slot) * 6.2831853f / static_cast<float>(SLOTS);
            x = std::cos(angle) * RING;
            y = std::sin(angle) * RING;
        }

        // Qué señala el cursor (x, y): una emisora (0..COUNT), -1 en el centro o -2 fuera de la rueda.
        inline int hit(float x, float y) {
            const float d2 = x * x + y * y;
            if (d2 < DEAD * DEAD) return -1;
            if (d2 > OUTER * OUTER) return -2;
            const float turn = (std::atan2(y, x) - 1.5707963f) / (6.2831853f / static_cast<float>(SLOTS));
            const int n = static_cast<int>(SLOTS);
            return ((static_cast<int>(std::lround(turn)) % n) + n) % n;
        }
    }
}
