#pragma once
#include <cstddef>

// Emisoras de radio del juego. Cada una es un fichero de assets/audio/radio/ (WAV mono 16 bit en bucle). Para cambiar la música de una
// emisora basta con sustituir su fichero; para añadir otra, una fila aquí. La emisora 0 es "apagada".
namespace Radio {
    struct Station { const char* name; const char* genre; const char* path; };

    inline constexpr size_t COUNT = 10;

    inline const Station& station(int number) { // 1..COUNT
        static const Station table[COUNT] = {
            { "Ruta 1 FM",           "Pop",         "audio/radio/01_pop.wav" },
            { "Lofi Centro Pokémon", "Lofi",        "audio/radio/02_lofi.wav" },
            { "Kanto Rock",          "Rock",        "audio/radio/03_rock.wav" },
            { "Chiptune 8-bit",      "Chiptune",    "audio/radio/04_chip.wav" },
            { "Jazz Nocturno",       "Jazz",        "audio/radio/05_jazz.wav" },
            { "Neón Electro",        "Electrónica", "audio/radio/06_electro.wav" },
            { "Reggae de la Playa",  "Reggae",      "audio/radio/07_reggae.wav" },
            { "Orquesta Épica",      "Orquestal",   "audio/radio/08_epic.wav" },
            { "Funk del Gimnasio",   "Funk",        "audio/radio/09_funk.wav" },
            { "Ambient del Cielo",   "Ambiental",   "audio/radio/10_ambient.wav" },
        };
        return table[(number < 1 ? 1 : number > static_cast<int>(COUNT) ? static_cast<int>(COUNT) : number) - 1];
    }

    // Emisora siguiente o anterior (0 = apagada; ciclo: apagada → 1 → ... → COUNT → apagada).
    inline int cycle(int current, int step) {
        const int n = static_cast<int>(COUNT) + 1;
        return ((current + step) % n + n) % n;
    }
}
