#pragma once
#include <cstdio>
#include <stdexcept>
#include <string>
#include <windows.h>
#include "../core/loggerUtil.hpp"

namespace D3dUtil {
    // Comprueba un HRESULT: si falla, lo registra en el log y lanza excepción.
    inline void check(HRESULT hr, const char* what) {
        if (FAILED(hr)) {
            char hex[16];
            std::snprintf(hex, sizeof(hex), "0x%08X", static_cast<unsigned>(hr));
            const std::string message = std::string(what) + " (HRESULT " + hex + ")";
            Logger::logError("D3D", message);
            throw std::runtime_error(message);
        }
    }
}