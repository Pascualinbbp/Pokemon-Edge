#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace StringUtil {
    // Escribe con formato printf en un buffer de pila (sin reservar memoria) y devuelve el buffer.
    // Pensado para texto que cambia en cada frame de la interfaz.
    template <size_t N>
    inline const char* formatTo(char (&buffer)[N], const char* format, ...) {
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, N, format, args);
        va_end(args);
        return buffer;
    }

    // Entre comillas simples, como espera PowerShell.
    inline std::wstring quote(const std::wstring& text) {
        return L"'" + text + L"'";
    }

    inline std::wstring join(const std::vector<std::wstring>& items, const std::wstring& separator) {
        std::wstring result;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) result += separator;
            result += items[i];
        }
        return result;
    }
}