#pragma once
#include <ctime>
#include <string>

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
}