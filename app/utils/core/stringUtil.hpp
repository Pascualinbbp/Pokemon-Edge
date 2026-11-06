#pragma once
#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace StringUtil {
    // Nombre de carpeta o fichero a partir de un texto de la base de datos: minúsculas, sin tildes y con '_' en vez de espacios ("Piedras evolutivas" -> "piedras_evolutivas").
    inline std::string slug(const std::string& name) {
        static const std::pair<const char*, char> ACCENTS[] = { { "á", 'a' }, { "é", 'e' }, { "í", 'i' }, { "ó", 'o' }, { "ú", 'u' }, { "ñ", 'n' }, { "ü", 'u' } };
        std::string result;
        for (size_t i = 0; i < name.size();) {
            bool replaced = false;
            for (const auto& accent : ACCENTS) {
                const size_t length = std::char_traits<char>::length(accent.first);
                if (name.compare(i, length, accent.first) == 0) { result += accent.second; i += length; replaced = true; break; }
            }
            if (replaced) continue;
            const unsigned char c = static_cast<unsigned char>(name[i++]);
            result += c == ' ' ? '_' : static_cast<char>(std::tolower(c));
        }
        return result;
    }

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