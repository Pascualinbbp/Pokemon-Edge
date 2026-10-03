#pragma once
#include <algorithm>
#include <ctime>
#include <string>
#include <windows.h>

namespace TimeUtil {
    // Fecha y hora local con formato "03/10/2026 02:14". Devuelve "" si no hay fecha válida.
    inline std::string formatLocal(std::time_t time) {
        if (time <= 0) return {};

        std::tm local = {};
        if (localtime_s(&local, &time) != 0) return {};

        char buffer[32];
        const size_t length = std::strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &local);
        return std::string(buffer, length);
    }

    // Cronómetro de frames con el contador de alta resolución.
    class FrameTimer {
        public:
        FrameTimer() {
            QueryPerformanceFrequency(&m_frequency);
            reset();
        }

        // Reinicia la cuenta (por ejemplo, tras una espera en la que no se dibujó nada).
        void reset() { QueryPerformanceCounter(&m_last); }

        // Segundos desde la última llamada, limitados a maxSeconds para que una pausa larga no dispare la simulación.
        float tick(float maxSeconds) {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            const float seconds = static_cast<float>(now.QuadPart - m_last.QuadPart) / static_cast<float>(m_frequency.QuadPart);
            m_last = now;
            return (std::min)(seconds, maxSeconds);
        }

        private:
        LARGE_INTEGER m_frequency = {};
        LARGE_INTEGER m_last = {};
    };
}