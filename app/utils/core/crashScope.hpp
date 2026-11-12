#pragma once

// Pila de funciones con la línea por la que va cada una (solo el hilo principal), sin dependencias de Windows.
// CRASH_SCOPE() al entrar en una función y CRASH_AT() para marcar un punto. Si el juego falla, CrashUtil escribe en el registro exactamente
// en qué funciones y líneas estaba, sin necesidad de símbolos.
namespace CrashScope {
    inline constexpr int MAX = 24;
    inline const char* names[MAX] = {};
    inline int lines[MAX] = {};
    inline volatile int depth = 0;

    struct Guard {
        Guard(const char* name, int line) { const int d = depth; if (d < MAX) { names[d] = name; lines[d] = line; } depth = d + 1; }
        ~Guard() { --depth; }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    };
    inline void at(int line) { const int d = depth - 1; if (d >= 0 && d < MAX) lines[d] = line; }
}

#define CRASH_SCOPE() CrashScope::Guard crashScope_(__FUNCTION__, __LINE__)
#define CRASH_AT() CrashScope::at(__LINE__)
